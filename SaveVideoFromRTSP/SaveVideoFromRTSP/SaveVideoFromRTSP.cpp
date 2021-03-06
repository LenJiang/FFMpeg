#include "stdafx.h"
#include "iostream"
#include "pch.h"  
#include <string>   

using namespace std;

AVFormatContext *inputAVFormatContext = nullptr;
AVCodecContext *inputAVCodecContext = nullptr;
AVFormatContext *outputAVFormatContext = nullptr;
AVCodecContext *outPutAVCodecContext = nullptr;
struct SwsContext *swsContex = nullptr;
AVRational inputTimeBase;
int inputVideoStream;
string inputStreamUrl;
string outputStreamUrlBase;
int64_t WritePacketNum = 0;
int64_t startTime;

void initEnv() {

	av_register_all();
	avformat_network_init();
	av_log_set_level(AV_LOG_INFO);

	//inputStreamUrl = "rtsp://192.168.3.34/tc10.264";
	inputStreamUrl = "rtsp://192.168.3.34/test.264";
	outputStreamUrlBase = "G:\\Microsoft\\VisualStudio\\tmp\\";
}


static int interruptCallback(void *ctx)
{
	int  timeout = 10;
	if (av_gettime() - startTime > timeout * 1000 * 1000)
	{
		av_log(nullptr, AV_LOG_FATAL, "inputstream blocking operation is timeout(>10s)!\n");
		return 1;
	}
	else {
		startTime = av_gettime();
		//av_log(nullptr, AV_LOG_FATAL, "not timeout(<10s)!\n");
		return 0;
	}
}

int initInput(string inputURL) {

	//open the input stream and read the herder.The codecs are not opened
	inputAVFormatContext = avformat_alloc_context();
	startTime = av_gettime();
	inputAVFormatContext->interrupt_callback.callback = interruptCallback;
	int ret = avformat_open_input(&inputAVFormatContext, (const char *)inputURL.c_str(), nullptr, nullptr);
	if (ret < 0) {
		av_log(nullptr, AV_LOG_ERROR, " Open The input stream and read the header failed!(%s)\n", inputURL.c_str());
		return ret;
	}
	else {
		av_log(nullptr, AV_LOG_FATAL, "Open The input stream and read the header success!\n");
	}

	//Initialize the AVCodecContext to use the given(AVMEDIA_TYPE_VIDEO,we just decode video part later) AVCodec,
	inputVideoStream = av_find_best_stream(inputAVFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	if (inputVideoStream < 0) {
		av_log(nullptr, AV_LOG_ERROR, "find Video stream failed in the input stream !\n");
		return -1;
	}
	else {
		av_log(nullptr, AV_LOG_FATAL, "find Video stream success in the input stream !\n");
	}

	inputTimeBase = inputAVFormatContext->streams[inputVideoStream]->time_base;
	AVCodec *avCodec = avcodec_find_decoder(inputAVFormatContext->streams[inputVideoStream]->codecpar->codec_id);
	inputAVCodecContext = avcodec_alloc_context3(avCodec);
	avcodec_parameters_to_context(inputAVCodecContext, inputAVFormatContext->streams[inputVideoStream]->codecpar);

	ret = avcodec_open2(inputAVCodecContext, avCodec, nullptr);
	if (ret < 0) {
		av_log(nullptr, AV_LOG_ERROR, "Initialize the input stream AVCodecContext to use the given AVCodec(AVMEDIA_TYPE_VIDEO) failed!\n");
		return ret;
	}
	else {
		av_log(nullptr, AV_LOG_FATAL, "Initialize the input stream AVCodecContext to use the given AVCodec(AVMEDIA_TYPE_VIDEO) success!\n");
	}
	return 0;
}

int initOutput(const char* outputURL, AVRational timebase, AVCodecContext *inputCC) {
	if ((outputURL == nullptr) || (inputCC == nullptr)) {
		av_log(nullptr, AV_LOG_ERROR, "output paramaters is nullptr");
		return -1;
	}

	AVCodec *videoCodec;
	videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	outPutAVCodecContext = avcodec_alloc_context3(videoCodec);
	outPutAVCodecContext->codec_id = videoCodec->id;
	outPutAVCodecContext->time_base.num = timebase.num;
	outPutAVCodecContext->time_base.den = timebase.den;
	outPutAVCodecContext->pix_fmt = *videoCodec->pix_fmts;
	//outPutAVCodecContext->width = inputCC->width;
	//outPutAVCodecContext->height = inputCC->height;
	outPutAVCodecContext->width = inputCC->width * 2;
	outPutAVCodecContext->height = inputCC->height * 2;
	int ret = avcodec_open2(outPutAVCodecContext, videoCodec, nullptr);
	if (ret < 0) {
		av_log(nullptr, AV_LOG_ERROR, "Initialize the out stream AVCodecContext  to use the given AVCodec(AV_CODEC_ID_H264) failed!\n");
		return ret;
	}
	else {
		av_log(nullptr, AV_LOG_FATAL, "Initialize the out stream AVCodecContext  to use the given AVCodec(AV_CODEC_ID_H264) success!\n");
	}

	ret = avformat_alloc_output_context2(&outputAVFormatContext, nullptr, nullptr, outputURL);
	if (ret < 0) {
		av_log(nullptr, AV_LOG_ERROR, outputURL);
		av_log(nullptr, AV_LOG_ERROR, "Allocate an AVFormatContext failed for the outputURL!(%s)*\n", outputURL);
		return ret;
	}
	else {
		av_log(nullptr, AV_LOG_FATAL, "Allocate an AVFormatContext success for the outputURL!(%s)*\n", outputURL);
	}

	ret = avio_open(&(outputAVFormatContext->pb), outputURL, AVIO_FLAG_WRITE);
	if (ret < 0) {
		av_log(nullptr, AV_LOG_ERROR, "Create and initialize a AVIOContext failed for accessing the resource indicated by the outputUrl!\n");
		return ret;
	}
	else {
		av_log(nullptr, AV_LOG_FATAL, "Create and initialize a AVIOContext success for accessing the resource indicated by the outputUrl!\n");
	}

	AVStream *outAvStream = avformat_new_stream(outputAVFormatContext, videoCodec);
	if (outAvStream == nullptr) {
		av_log(nullptr, AV_LOG_ERROR, "Add a output stream to a media file for mux later failed!\n");
		return -1;
	}
	else {
		av_log(nullptr, AV_LOG_FATAL, "Add a output stream to a media file for mux later  success!\n");
	}
	avcodec_parameters_from_context(outputAVFormatContext->streams[0]->codecpar, outPutAVCodecContext);
	return 0;
}

int decodeSend(AVCodecContext *inputCC, AVPacket *packet, int packetIndex) {
	int ret = avcodec_send_packet(inputCC, packet);
	if (ret < 0) {
		if (ret == AVERROR(EAGAIN)) {
			av_log(nullptr, AV_LOG_ERROR, "%d send packet failed( errcode = AVERROR(EAGAIN) )!\n", packetIndex);
		}
		else if (ret == AVERROR_EOF) {
			av_log(nullptr, AV_LOG_ERROR, "%d send packet failed( errcode = AVERROR_EOF )!\n", packetIndex);
		}
		else {
			av_log(nullptr, AV_LOG_ERROR, "%d send packet failed( errcode = %d )!\n", packetIndex, ret);
		}
		return ret;
	}
	else {
		if (packetIndex % 20 == 0)
			av_log(nullptr, AV_LOG_FATAL, "%d send packet success(%d)\n", packetIndex, packet->pts);
	}
	return 0;
}

int decodeReceive(AVCodecContext *inputCC, AVFrame *frame, int packetIndex) {
	av_frame_unref(frame);
	int ret = avcodec_receive_frame(inputCC, frame);
	if (ret < 0) {
		if (ret == AVERROR(EAGAIN)) {
			av_log(nullptr, AV_LOG_ERROR, "%d receive frame failed( errcode = AVERROR(EAGAIN) )!\n", packetIndex);
		}
		else if (ret == AVERROR_EOF) {
			av_log(nullptr, AV_LOG_ERROR, "%d receive frame failed( errcode = AVERROR_EOF )!\n", packetIndex);
		}
		else {
			av_log(nullptr, AV_LOG_ERROR, "%d receive frame failed( errcode = %d )!\n", packetIndex, ret);
		}
		return ret;
	}
	else {
		if (packetIndex % 20 == 0) {
			av_log(nullptr, AV_LOG_FATAL, "%d receive frame success(%d)\n", packetIndex, frame->best_effort_timestamp);
		}
	}
	return 0;
}

int encodeSend(AVCodecContext *outCC, AVFrame *inputFrame, AVFrame *outFrame, int packetIndex) {
	if (outFrame != nullptr)
		sws_scale(swsContex, (const uint8_t *const *)inputFrame->data, inputFrame->linesize, 0, inputFrame->height, (uint8_t *const *)outFrame->data, outFrame->linesize);
	int ret = avcodec_send_frame(outCC, outFrame);
	if (ret < 0) {
		if (ret == AVERROR(EAGAIN)) {
			av_log(nullptr, AV_LOG_ERROR, "%d send frame failed( errcode = AVERROR(EAGAIN) )!\n", packetIndex);
		}
		else if (ret == AVERROR_EOF) {
			av_log(nullptr, AV_LOG_ERROR, "%d send frame failed( errcode = AVERROR_EOF )!\n", packetIndex);
		}
		else {
			av_log(nullptr, AV_LOG_ERROR, "%d send frame failed( errcode = %d )!\n", packetIndex, ret);
		}
		return ret;
	}
	else {
		if (packetIndex % 20 == 0) {
			if (outFrame != nullptr)
				av_log(nullptr, AV_LOG_FATAL, "%d send frame success(%d)\n", packetIndex, outFrame->best_effort_timestamp);
		}
	}
	return 0;
}

int encodeReceive(AVCodecContext *outCC, AVPacket *packet, int packetIndex) {
	av_packet_unref(packet);
	int ret = avcodec_receive_packet(outCC, packet);
	if (ret < 0) {
		if (ret == AVERROR(EAGAIN)) {
			av_log(nullptr, AV_LOG_ERROR, "%d receive packet failed( errcode = AVERROR(EAGAIN) )!\n", packetIndex);
		}
		else if (ret == AVERROR_EOF) {
			av_log(nullptr, AV_LOG_ERROR, "%d receive packet failed( errcode = AVERROR_EOF )!\n", packetIndex);
		}
		else {
			av_log(nullptr, AV_LOG_ERROR, "%d receive packet failed( errcode = %d )!\n", packetIndex, ret);
		}
		return ret;
	}
	else {
		if (packetIndex % 20 == 0)
			av_log(nullptr, AV_LOG_FATAL, "%d receive packet success(%d)\n", packetIndex, packet->pts);
	}
	return 0;
}

int writeHeader(AVFormatContext *outFX) {
	int ret = avformat_write_header(outFX, nullptr);
	if (ret < 0) {
		av_log(nullptr, AV_LOG_ERROR, "Allocate the stream private data and write the stream header to the media file failed!\n");
		return ret;
	}
	else {
		av_log(nullptr, AV_LOG_FATAL, "Allocate the stream private data and write the stream header to the media file success!\n");
	}
	return 0;
}

int writepacket(AVFormatContext *outFX, AVPacket *packet, AVFormatContext *inputFX, AVFrame *inputFrame) {
	//int64_t tmp = packetIndex * outFX->streams[0]->time_base.den / outFX->streams[0]->time_base.num / 30;
	//get src timestamp
	//int64_t scrTime = av_frame_get_best_effort_timestamp(inputFrame)*av_q2d(inputFX->streams[0]->time_base);
	//get dst timestamp
	//int64_t dstTime = scrTime / av_q2d(outFX->streams[0]->time_base);
	//packet->pts = packet->dts = av_rescale_q(av_frame_get_best_effort_timestamp(inputFrame), inputFX->streams[0]->time_base, outFX->streams[0]->time_base);
	WritePacketNum++;
	//asume r_frame_rate = 25 or get from outFX->streams[0]->r_frame_rate,if is not 0;
	packet->pts = packet->dts = WritePacketNum * outFX->streams[0]->time_base.den / outFX->streams[0]->time_base.num / 25;
	int ret = av_interleaved_write_frame(outFX, packet);
	if (ret < 0) {
		av_log(nullptr, AV_LOG_ERROR, "%d Write a packet to the output media file failed!(errorCode = %d)\n", WritePacketNum, ret);
		return ret;
	}
	else {
		if (WritePacketNum % 20 == 0)
			av_log(nullptr, AV_LOG_FATAL, "Write a packet to the output media file success!\n");
	}
	return ret;
}

int initSws(SwsContext **swsC, AVCodecContext *inputCC, AVCodecContext *outputCC, AVFrame *outputF) {
	*swsC = sws_getContext(inputCC->width, inputCC->height, inputCC->pix_fmt, outputCC->width, outputCC->height, outputCC->pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr);
	if (*swsC == nullptr) {
		av_log(nullptr, AV_LOG_ERROR, "get SWSContext failed!\n");
		return -1;
	}

	int numBytes = av_image_get_buffer_size(outputCC->pix_fmt, outputCC->width, outputCC->height, 1);
	if (numBytes < 0) {
		av_log(nullptr, AV_LOG_ERROR, "get Image buffer failed!\n");
		return numBytes;
	}

	const uint8_t *outputSwpBuffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
	int ret = av_image_fill_arrays(outputF->data, outputF->linesize, outputSwpBuffer, outputCC->pix_fmt, outputCC->width, outputCC->height, 1);
	if (ret < 0) {
		av_log(nullptr, AV_LOG_ERROR, "set outputFrame data point and linesize failed!\n");
		return ret;
	}
	outputF->width = outputCC->width;
	outputF->height = outputCC->height;
	outputF->format = outputCC->pix_fmt;
	return 0;
}


void releaseInputResource() {
	if (inputAVFormatContext != nullptr)
	{
		avformat_free_context(inputAVFormatContext);
	}
}


void releaseOutputResource() {
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
	if (outputAVFormatContext != nullptr)
	{
		avformat_free_context(outputAVFormatContext);
	}
}

//int _tmain(int argc, _TCHAR* argv[]) {
//int main(int argc, char* argv[]) {
int main() {

	initEnv();

	int ret = initInput(inputStreamUrl);
	if (ret < 0) {
		closeInput();
		releaseInputResource();
		return ret;
	}
	av_dump_format(inputAVFormatContext, 0, inputStreamUrl.c_str(), 0);

	AVPacket *avPacket = (AVPacket *)av_malloc(sizeof(AVPacket));
	AVFrame *inputAVFrame = av_frame_alloc();
	AVFrame *outputAVFrame = av_frame_alloc();
	int avPacketIndex = 0;
	int64_t startTime = av_gettime();
	bool isOuputInited = false;
	while (true) {
		ret = av_read_frame(inputAVFormatContext, avPacket);
		if (ret >= 0) {
			/*if (av_gettime() - startTime > 60000000) {
				break;
			}*/
			if (inputVideoStream == avPacket->stream_index) {
				avPacketIndex++;
				ret = decodeSend(inputAVCodecContext, avPacket, avPacketIndex);
				ret = decodeReceive(inputAVCodecContext, inputAVFrame, avPacketIndex);
				av_packet_unref(avPacket);
				if (ret < 0) {
					continue;
				}
				if (!isOuputInited) {
					string outputStreamUrl = outputStreamUrlBase;
					outputStreamUrl.append("rtsp2h264.mp4");
					ret = initOutput(outputStreamUrl.c_str(), inputTimeBase, inputAVCodecContext);
					if (ret < 0) {
						break;
					}

					ret = writeHeader(outputAVFormatContext);
					if (ret < 0) {
						break;
					}

					ret = initSws(&swsContex, inputAVCodecContext, outPutAVCodecContext, outputAVFrame);
					if (ret < 0) {
						break;
					}
					isOuputInited = true;
				}
				ret = encodeSend(outPutAVCodecContext, inputAVFrame, outputAVFrame, avPacketIndex);
				ret = encodeReceive(outPutAVCodecContext, avPacket, avPacketIndex);
				if (ret < 0) {
					continue;
				}
				ret = writepacket(outputAVFormatContext, avPacket, inputAVFormatContext, inputAVFrame);
				av_packet_unref(avPacket);
				if (ret < 0) {
					continue;
				}
			}
			else {
				//printf("%d audio packet\n",index);
			}
		}
		else {
			if (ret == AVERROR_EOF) {
				av_log(nullptr, AV_LOG_ERROR, "read frame in the input stream end of the file!\n");
				//first called,ret=0;later called, ret=AVERROR_EOF;
				ret = decodeSend(inputAVCodecContext, avPacket, avPacketIndex);
				ret = decodeReceive(inputAVCodecContext, inputAVFrame, avPacketIndex);
				if (ret >= 0) {
					ret = encodeSend(outPutAVCodecContext, inputAVFrame, outputAVFrame, avPacketIndex);
				}
				else {
					//send a NULL,This signals the end of the stream
					ret = encodeSend(outPutAVCodecContext, inputAVFrame, nullptr, avPacketIndex);
				}
				ret = encodeReceive(outPutAVCodecContext, avPacket, avPacketIndex);
				if (ret < 0) {
					if (ret == AVERROR_EOF) {
						break;
					}
					continue;
				}
				else {
					ret = writepacket(outputAVFormatContext, avPacket, inputAVFormatContext, inputAVFrame);
				}
			}
			else {
				av_log(nullptr, AV_LOG_ERROR, "read frame in the input stream on error!\n");
				break;
			}
		}
	}
	av_frame_free(&inputAVFrame);
	av_frame_free(&outputAVFrame);
	closeInput();
	if (swsContex != nullptr) {
		sws_freeContext(swsContex);
	}
	closeoutput();
	releaseOutputResource();
	releaseInputResource();
	return 0;
}

/*
avcodec.lib
avdevice.lib
avfilter.lib
avformat.lib
avutil.lib
postproc.lib
swresample.lib
swscale.lib
*/