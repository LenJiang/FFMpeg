#include "stdafx.h"
#include "iostream"
#include "pch.h"

using namespace std;

AVFormatContext *inputAVFormatContext;
AVCodecContext *inputAVCodecContext;
AVFormatContext *outputAVFormatContext;
AVCodecContext *outPutAVCodecContext;
AVRational inputTimeBase;
int inputVideoStream;

int initInput(string inputURL) {

	//open the input stream and read the herder.The codecs are not opened
	inputAVFormatContext = avformat_alloc_context();
	int ret = avformat_open_input(&inputAVFormatContext, (const char *)inputURL.c_str(), NULL, NULL);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, " Open The input stream and read the header failed!(%s)\n", inputURL.c_str());
		return ret;
	}
	else {
		av_log(NULL, AV_LOG_FATAL, "Open The input stream and read the header success!\n");
	}

	//Initialize the AVCodecContext to use the given(AVMEDIA_TYPE_VIDEO,we just decode viadeo part later) AVCodec,
	inputVideoStream = av_find_best_stream(inputAVFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (inputVideoStream < 0) {
		av_log(NULL, AV_LOG_ERROR, "find Video stream failed in the input stream !\n");
		return -1;
	}
	else {
		av_log(NULL, AV_LOG_FATAL, "find Video stream success in the input stream !\n");
	}
	
	inputTimeBase = inputAVFormatContext->streams[inputVideoStream]->time_base;
	AVCodec *avCodec = avcodec_find_decoder(inputAVFormatContext->streams[inputVideoStream]->codecpar->codec_id);
	inputAVCodecContext = avcodec_alloc_context3(avCodec);
	avcodec_parameters_to_context(inputAVCodecContext, inputAVFormatContext->streams[inputVideoStream]->codecpar);

	ret = avcodec_open2(inputAVCodecContext, avCodec, NULL);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Initialize the input stream AVCodecContext to use the given AVCodec(AVMEDIA_TYPE_VIDEO) failed!\n");
		return ret;
	}
	else {
		av_log(NULL, AV_LOG_FATAL, "Initialize the input stream AVCodecContext to use the given AVCodec(AVMEDIA_TYPE_VIDEO) success!\n");
	}
	return 0;
}

int initOutput(const char* outputURL, AVRational timebase, AVCodecContext *inputCC) {
	if((outputURL == nullptr)  || (inputCC == nullptr)){
		av_log(NULL, AV_LOG_ERROR, "output paramaters is null");
		return -1;
	}

	int ret = avformat_alloc_output_context2(&outputAVFormatContext, NULL, NULL, outputURL);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, outputURL);
		av_log(NULL, AV_LOG_ERROR, "Allocate an AVFormatContext failed for the outputURL!(%s)*\n", outputURL);
		return ret;
	}
	else {
		av_log(NULL, AV_LOG_FATAL, "Allocate an AVFormatContext success for the outputURL!(%s)*\n", outputURL);
	}

	ret = avio_open(&(outputAVFormatContext->pb), outputURL, AVIO_FLAG_WRITE);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Create and initialize a AVIOContext failed for accessing the resource indicated by the outputUrl!\n");
		return ret;
	}
	else {
		av_log(NULL, AV_LOG_FATAL, "Create and initialize a AVIOContext success for accessing the resource indicated by the outputUrl!\n");
	}

	AVCodec *picCodec;
	picCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
	AVStream *outAvStream = avformat_new_stream(outputAVFormatContext, picCodec);
	if (outAvStream == NULL) {
		av_log(NULL, AV_LOG_ERROR, "Add a output stream to a media file for mux later failed!\n");
		return -1;
	}
	else {
		av_log(NULL, AV_LOG_FATAL, "Add a output stream to a media file for mux later  success!\n");
	}

	outPutAVCodecContext = avcodec_alloc_context3(picCodec);
	outPutAVCodecContext->codec_id = picCodec->id;
	outPutAVCodecContext->time_base.num = timebase.num;
	outPutAVCodecContext->time_base.den = timebase.den;
	outPutAVCodecContext->pix_fmt = *picCodec->pix_fmts;
	outPutAVCodecContext->width = inputCC->width;
	outPutAVCodecContext->height = inputCC->height;
	ret = avcodec_open2(outPutAVCodecContext, picCodec, NULL);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Initialize the out stream AVCodecContext  to use the given AVCodec(AV_CODEC_ID_MJPEG) failed!\n");
		return ret;
	}
	else {
		av_log(NULL, AV_LOG_FATAL, "Initialize the out stream AVCodecContext  to use the given AVCodec(AV_CODEC_ID_MJPEG) success!\n");
	}

	return 0;
}

int decode(AVCodecContext *inputCC, AVPacket *packet, AVFrame *frame, int packetIndex) {
	int ret = avcodec_send_packet(inputCC, packet);
	if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
		av_log(NULL, AV_LOG_ERROR, "send packet failed!\n");
		return ret;
	}
	else {
		if (packetIndex % 100 == 0)
			av_log(NULL, AV_LOG_FATAL, "%d send packet success(%d)\n", packetIndex, packet->dts);
	}
	av_frame_unref(frame);

	ret = avcodec_receive_frame(inputCC, frame);
	if (ret < 0 &&
		ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
		av_log(NULL, AV_LOG_ERROR, "receive  frame fail\n");
		return ret;
	}
	else {
		if (packetIndex % 100 == 0) {
			av_log(NULL, AV_LOG_FATAL, "%d receive frame success(%d)\n", packetIndex, frame->best_effort_timestamp);
		}
	}
	return 0;
}

int encode(AVCodecContext *outCC, AVPacket *packet, AVFrame *frame, int packetIndex) {
	int ret = avcodec_send_frame(outCC, frame);
	if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
		av_log(NULL, AV_LOG_ERROR, "send frame fail\n");
		return ret;
	}
	else {
		if (packetIndex % 100 == 0) {
			av_log(NULL, AV_LOG_FATAL, "%d send frame success(%d)\n", packetIndex, frame->best_effort_timestamp);
		}
	}
	av_packet_unref(packet);
	ret = avcodec_receive_packet(outCC, packet);
	if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
		av_log(NULL, AV_LOG_ERROR, "recieve packet fail\n");
		return ret;
	}
	else {
		if (packetIndex % 100 == 0)
			av_log(NULL, AV_LOG_FATAL, "%d receive packet success(%d)\n", packetIndex, packet->dts);
	}
	return 0;
}

int writeHeader(AVFormatContext *outFX) {
	int ret = avformat_write_header(outFX, NULL);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Allocate the stream private data and write the stream header to the media file failed!\n");
		return ret;
	}
	else {
		av_log(NULL, AV_LOG_FATAL, "Allocate the stream private data and write the stream header to the media file success!\n");
	}
	return 0;
}

int writepacket(AVFormatContext *outFX, AVPacket *packet) {
	int ret = av_interleaved_write_frame(outFX, packet); 
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Write a packet to the output media file failed!\n");
		return ret;
	}
	else {
		av_log(NULL, AV_LOG_FATAL, "Write a packet to the output media file success!\n");
	}
	return ret;
}

void releaseResource() {
	if (inputAVFormatContext != nullptr)
	{
		avformat_free_context(inputAVFormatContext);
	}
	if (outputAVFormatContext != nullptr)
	{
		avformat_free_context(outputAVFormatContext);
	}
}

void closeInput()
{
	if (inputAVFormatContext != nullptr)
	{
		avformat_close_input(&inputAVFormatContext);
	}
	if (inputAVCodecContext != nullptr) {
		avcodec_close(inputAVCodecContext);
	}
}

void closeoutput()
{
	if (outputAVFormatContext != nullptr)
	{
		int ret = av_write_trailer(outputAVFormatContext);
		avformat_close_input(&outputAVFormatContext);
	}
	if (outPutAVCodecContext != nullptr) {
		avcodec_close(outPutAVCodecContext);
	}
}

//int _tmain(int argc, _TCHAR* argv[]) {
//int main(int argc, char* argv[]) {
int main() {

	av_register_all();
	av_log_set_level(AV_LOG_ERROR);

	string inputStreamUrl = "G:\\Microsoft\\VisualStudio\\tmp\\test1.mp4";
	string outputStreamUrl = "G:\\Microsoft\\VisualStudio\\tmp\\test.jpg";

	int ret = initInput(inputStreamUrl);
	if (ret < 0) {
		closeInput();
		releaseResource();
		return -1;
	}
	ret = initOutput(outputStreamUrl.c_str(), inputTimeBase, inputAVCodecContext);
	if (ret < 0) {
		closeInput();
		closeoutput();
		releaseResource();
		return -1;
	}
	ret = writeHeader(outputAVFormatContext);
	if (ret < 0) {
		closeInput();
		closeoutput();
		releaseResource();
		return -1;
	}

	AVPacket *avPacket = (AVPacket *)av_malloc(sizeof(AVPacket));
	AVFrame *avFrame = av_frame_alloc();
	int avPacketIndex = 0;
	while (av_read_frame(inputAVFormatContext, avPacket) >= 0) {
		if (inputVideoStream == avPacket->stream_index) {
			avPacketIndex++;
			ret = decode(inputAVCodecContext, avPacket, avFrame, avPacketIndex);
			if (ret < 0) {
				av_packet_unref(avPacket);
				continue;
			}
			if (avPacketIndex == 100) {
				ret = encode(outPutAVCodecContext, avPacket, avFrame, avPacketIndex);
				if (ret < 0) {
					av_packet_unref(avPacket);
					continue;
				}
				ret = writepacket(outputAVFormatContext, avPacket); 
				if (ret < 0) {
					av_packet_unref(avPacket);
					continue;
				}
			}
			av_packet_unref(avPacket);
		}
		else {
			//printf("%d audio packet\n",index);
		}
	}
	av_frame_free(&avFrame);
	closeInput();
	closeoutput();
	releaseResource();
	return 0;
}