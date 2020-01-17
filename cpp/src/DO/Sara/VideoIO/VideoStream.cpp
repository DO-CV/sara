// ========================================================================== //
// This file is part of Sara, a basic set of libraries in C++ for computer
// vision.
//
// Copyright (C) 2015-2016 David Ok <david.ok8@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at http://mozilla.org/MPL/2.0/.
// ========================================================================== //

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>

#include <libavcodec/avcodec.h>
}

#include "VideoStream.hpp"
#include <DO/Sara/Core/DebugUtilities.hpp>

#include <cstdio>
#include <cstdlib>


namespace DO::Sara {

  inline static Yuv8 get_yuv_pixel(const AVFrame* frame, int x, int y)
  {
    Yuv8 yuv;
    yuv(0) = frame->data[0][y * frame->linesize[0] + x];
    yuv(1) = frame->data[1][y / 2 * frame->linesize[1] + x / 2];
    yuv(2) = frame->data[2][y / 2 * frame->linesize[2] + x / 2];
    return yuv;
  }

  inline static unsigned char clamp(int value)
  {
    if (value < 0)
      return 0;
    if (value > 255)
      return 255;
    return value;
  }

  inline static Rgb8 convert(const Yuv8& yuv)
  {
    Rgb8 rgb;
    int C = yuv(0) - 16;
    int D = yuv(1) - 128;
    int E = yuv(2) - 128;
    rgb(0) = clamp((298 * C + 409 * E + 128) >> 8);
    rgb(1) = clamp((298 * C - 100 * D - 208 * E + 128) >> 8);
    rgb(2) = clamp((298 * C + 516 * D + 128) >> 8);
    return rgb;
  }

}  // namespace DO::Sara


namespace DO::Sara {

  bool VideoStream::_registered_all_codecs = false;
  bool VideoStream2::_registered_all_codecs = false;

  VideoStream::VideoStream()
  {
    if (!_registered_all_codecs)
    {
      av_register_all();
      _registered_all_codecs = true;
    }
  }

  VideoStream::VideoStream(const std::string& file_path)
    : VideoStream()
  {
    open(file_path);
  }

  VideoStream::~VideoStream()
  {
    close();
  }

  int VideoStream::width() const
  {
    return _video_codec_context->width;
  }

  int VideoStream::height() const
  {
    return _video_codec_context->height;
  }

  void VideoStream::open(const std::string& file_path)
  {
    // Read the video file.
    if (avformat_open_input(&_video_format_context, file_path.c_str(), nullptr,
                            nullptr) != 0)
      throw std::runtime_error("Could not open video file!");

    // Read the video stream metadata.
    if (avformat_find_stream_info(_video_format_context, nullptr) != 0)
      throw std::runtime_error("Could not retrieve video stream info!");
    av_dump_format(_video_format_context, 0, file_path.c_str(), 0);

    // Retrieve video stream.
    _video_stream = -1;
    for (unsigned i = 0; i != _video_format_context->nb_streams; ++i)
    {
      if (_video_format_context->streams[i]->codec->codec_type ==
          AVMEDIA_TYPE_VIDEO)
      {
        _video_stream = i;
        break;
      }
    }
    if (_video_stream == -1)
      throw std::runtime_error("Could not retrieve video stream!");

    // Retrieve the video codec context.
    _video_codec_context = _video_format_context->streams[_video_stream]->codec;

    // Retrieve the video codec.
    _video_codec = avcodec_find_decoder(_video_codec_context->codec_id);
    if (!_video_codec)
      throw std::runtime_error("Could not find supported codec!");

    // Open the video codec.
    if (avcodec_open2(_video_codec_context, _video_codec, nullptr) < 0)
      throw std::runtime_error("Could not open video codec!");

    // Allocate video frame.
    _video_frame = av_frame_alloc();
    if (!_video_frame)
      throw std::runtime_error("Could not allocate video frame!");

    _video_frame_pos = 0;
  }

  void VideoStream::close()
  {
    if (_video_frame)
    {
      av_frame_free(&_video_frame);
      _video_frame = nullptr;
      _video_frame_pos = std::numeric_limits<size_t>::max();
    }

    if (_video_codec_context)
    {
      avcodec_close(_video_codec_context);
      avcodec_free_context(&_video_codec_context);
      _video_codec_context = nullptr;
      _video_codec = nullptr;
    }

    if (_video_format_context)
      _video_format_context = nullptr;
  }

  void VideoStream::seek(std::size_t frame_pos)
  {
    av_seek_frame(_video_format_context, _video_stream, frame_pos,
                  AVSEEK_FLAG_BACKWARD);
  }

  bool VideoStream::read(ImageView<Rgb8>& video_frame)
  {
    if (video_frame.sizes() != sizes())
      throw std::domain_error{
          "Video frame sizes and video stream sizes are not equal!"};

    AVPacket _video_packet;
    auto length = int{};
    auto got_video_frame = int{};
    auto video_frame_data = video_frame.data();

    while (av_read_frame(_video_format_context, &_video_packet) >= 0)
    {
      length = avcodec_decode_video2(_video_codec_context, _video_frame,
                                     &got_video_frame, &_video_packet);

      if (length < 0)
        return false;

      if (got_video_frame)
      {
        auto w = width();
        auto h = height();

        for (int y = 0; y < h; ++y)
        {
          for (int x = 0; x < w; ++x)
          {
            auto yuv = get_yuv_pixel(_video_frame, x, y);
            *video_frame_data = Sara::convert(yuv);
            ++video_frame_data;
          }
        }

        ++_video_frame_pos;
        return true;
      }
    }

    return false;
  }


  VideoStream2::VideoStream2()
  {
    if (!_registered_all_codecs)
    {
      av_register_all();
      _registered_all_codecs = true;
    }
  }

  VideoStream2::~VideoStream2()
  {
    close();
  }

  void VideoStream2::open(const std::string& file_path)
  {
    _file = fopen(file_path.c_str(), "rb");
    if (_file == nullptr)
      throw std::runtime_error("Could not open video file!");

    if (_pkt == nullptr)
    {
      _pkt = av_packet_alloc();
      if (_pkt == nullptr)
        throw std::runtime_error("Could not allocate video packet!");
      else
        SARA_DEBUG << "Allocated video packet!" << std::endl;
    }

    // Set end of buffer to 0.
    //
    // This ensures that no overreading happens for damaged MPEG streams.
    std::memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

    // Find the MPEG-1 video decoder.
    _codec = avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO);
    if (_codec == nullptr)
      throw std::runtime_error{"Could not find video codec!"};

    parser = av_parser_init(_codec->id);
    if (parser == nullptr)
      throw std::runtime_error{"Could not find video parser!"};

    c = avcodec_alloc_context3(_codec);

    _picture = av_frame_alloc();

    // For some codecs, such as msmpeg4 and mpeg4, width and height MUST be
    // initialized there because this information is not available in the
    // bitstream.
    //
    // Open it.
    if (avcodec_open2(c, _codec, NULL) < 0)
      throw std::runtime_error{"Could not open video codec!"};

    SARA_DEBUG << "Video context height = " << c->height << std::endl;
    SARA_DEBUG << "Video context width = " << c->width << std::endl;

    SARA_DEBUG << "Width = " << parser->coded_width << std::endl;
    SARA_DEBUG << "Height = " << parser->coded_height << std::endl;
  }

  void VideoStream2::close()
  {
    if (_file != nullptr)
    {
      fclose(_file);
      _file = nullptr;
    }

    // Flush the decoder (draining mode.)
    this->decode(c, _picture, nullptr);

    // Free the data structures.
    av_parser_close(parser);
    avcodec_free_context(&c);
    av_frame_free(&_picture);
    av_packet_free(&_pkt);

    // Reset the frame number.
    _current_frame_number = 0;
  }

  bool VideoStream2::read()
  {
    while (!feof(_file))
    {
      /* read raw data from the input file */
      data_size = fread(inbuf, 1, INBUF_SIZE, _file);
      if (data_size == 0)
        return false;

      /* use the parser to split the data into frames */
      data = inbuf;
      while (data_size > 0)
      {
        // Parse a packet.
        const auto input_bitstream_bytesize =
            av_parser_parse2(parser, c, &_pkt->data, &_pkt->size, data,
                             data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        // Sanity check.
        if (input_bitstream_bytesize < 0)
          throw std::runtime_error{"Error while parsing video buffer"};

        data += input_bitstream_bytesize;
        data_size -= input_bitstream_bytesize;
        if (_pkt->size)
        {
          if (this->decode(c, _picture, _pkt))
          {
            SARA_DEBUG << "Read frame number = " << c->frame_number
                       << std::endl;
            return true;
          }
        }
      }
    }

    return false;
  }

  auto VideoStream2::decode(AVCodecContext* dec_ctx, AVFrame* frame,
                            AVPacket* pkt) -> bool
  {
    auto ret = int{};

    // Transfer raw compressed video data to the packet.
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0)
      throw std::runtime_error{"Error sending a packet for decoding!"};

    while (ret >= 0)
    {
      // Decode the compressed video data into an uncompressed video frame.
      ret = avcodec_receive_frame(dec_ctx, frame);

      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        return false;

      if (ret < 0)
        throw std::runtime_error{"Error during decoding!"};

      // The video frame is fully decoded.
      SARA_DEBUG << "Fully decoded video frame!" << std::endl;
      SARA_DEBUG << "video frame sizes = "  //
                 << frame->width << "x" << frame->height << std::endl;
      SARA_DEBUG << "video linesize = "  //
                 << frame->linesize[0] << std::endl;
      return true;

    }

    return false;
  }

  auto VideoStream2::frame() const -> Image<Rgb8>
  {
    // return {reinterpret_cast<Rgb8*>(_picture->data[0]),
    //         {_picture->width, _picture->height}};

    const auto w = _picture->width;
    const auto h = _picture->height;

    auto frame = Image<Rgb8>{w, h};
    std::cout << frame.sizes().transpose() << std::endl;

    for (int y = 0; y < h; ++y)
    {
      for (int x = 0; x < w; ++x)
      {
        auto yuv = get_yuv_pixel(_picture, x, y);
        frame(x, y) = Sara::convert(yuv);
      }
    }

    return frame;
  }

  auto VideoStream2::frame_rate() const -> float
  {
    return static_cast<float>(c->framerate.num) / c->framerate.den;
  }

}  // namespace DO::Sara
