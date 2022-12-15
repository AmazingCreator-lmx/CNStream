/*************************************************************************
 * Copyright (C) [2022] by Cambricon, Inc. All rights reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *************************************************************************/

#ifndef MODULES_SOURCE_HANDLER_JPEG_MEM_HPP_
#define MODULES_SOURCE_HANDLER_JPEG_MEM_HPP_

#include <string>

#include "data_handler_util.hpp"
#include "data_source.hpp"

namespace cnstream {

class ESJpegMemHandlerImpl;
/*!
 * @class ESJpegMemHandler
 *
 * @brief ESJpegMemHandler is a class of source handler for Jpeg bitstreams in memory.
 */
class ESJpegMemHandler : public SourceHandler {
 public:
  /*!
   * @brief A constructor to construct a ESJpegMemHandler object.
   *
   * @param[in] module The data source module.
   * @param[in] stream_id The stream id of the stream.
   * @param[in] param The parameters of the handler.
   *
   * @return No return value.
   */
  explicit ESJpegMemHandler(DataSource *module, const std::string &stream_id, const ESJpegMemSourceParam &param);

  /*!
   * @brief The destructor of ESJpegMemHandler.
   *
   * @return No return value.
   */
  ~ESJpegMemHandler();
  /*!
   * @brief Opens source handler.
   *
   * @return Returns true if the source handler is opened successfully, otherwise returns false.
   */
  bool Open() override;
  /*!
   * @brief Closes source handler.
   *
   * @return No return value.
   */
  void Close() override;

  /*!
   * @brief Sends data in frame mode.
   *
   * @param[in] pkt The data packet.
   *
   * @return Returns 0 if this function writes data successfully.
   *         Returns -1 if it fails to writes data. The possible reason is the pkt is nullptr or decoding failed.
   *
   * @note Must write pkt to notify the handler it's the end of the stream,
   *       set the data of the pkt to nullptr or the size to 0.
   */
  int Write(ESJpegPacket *pkt);

 private:
  ESJpegMemHandlerImpl *impl_ = nullptr;
};  // class ESJpegMemHandler

}  // namespace cnstream

#endif  // MODULES_SOURCE_HANDLER_JPEG_MEM_HPP_
