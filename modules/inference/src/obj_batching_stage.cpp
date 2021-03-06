/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
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

#include "obj_batching_stage.hpp"
#include <cn_codec_common.h>
#include <cnrt.h>
#include <easyinfer/model_loader.h>
#include <glog/logging.h>
#include <memory>
#include <vector>
#include "cnstream_frame.hpp"
#include "cnstream_frame_va.hpp"
#include "infer_resource.hpp"
#include "infer_task.hpp"
#include "preproc.hpp"

namespace cnstream {

std::shared_ptr<InferTask> IOObjBatchingStage::Batching(std::shared_ptr<CNFrameInfo> finfo,
                                                        std::shared_ptr<CNInferObject> obj) {
  bool reserve_ticket = false;
  if (batch_idx_ + 1 == batchsize_) {
    // ready to next batch, do not reserve resource ticket.
    reserve_ticket = false;
  } else {
    // in one batch, reserve resource ticket to parallel.
    reserve_ticket = true;
  }
  QueuingTicket ticket = output_res_->PickUpTicket(reserve_ticket);
  auto bidx = batch_idx_;
  std::shared_ptr<InferTask> task = std::make_shared<InferTask>([this, ticket, finfo, obj, bidx]() -> int {
    QueuingTicket t = ticket;
    IOResValue value = this->output_res_->WaitResourceByTicket(&t);
    this->ProcessOneObject(finfo, obj, bidx, value);
    this->output_res_->DeallingDone();
    return 0;
  });
  task->task_msg = "infer task.";
  batch_idx_ = (batch_idx_ + 1) % batchsize_;
  return task;
}

CpuPreprocessingObjBatchingStage::CpuPreprocessingObjBatchingStage(std::shared_ptr<edk::ModelLoader> model,
                                                                   uint32_t batchsize,
                                                                   std::shared_ptr<ObjPreproc> preprocessor,
                                                                   std::shared_ptr<CpuInputResource> cpu_input_res)
    : IOObjBatchingStage(model, batchsize, cpu_input_res), preprocessor_(preprocessor) {}

CpuPreprocessingObjBatchingStage::~CpuPreprocessingObjBatchingStage() {}

void CpuPreprocessingObjBatchingStage::ProcessOneObject(std::shared_ptr<CNFrameInfo> finfo,
                                                        std::shared_ptr<CNInferObject> obj, uint32_t batch_idx,
                                                        const IOResValue& value) {
  std::vector<float*> net_inputs;
  for (auto it : value.datas) {
    net_inputs.push_back(reinterpret_cast<float*>(it.Offset(batch_idx)));
  }
  preprocessor_->Execute(net_inputs, model_, finfo, obj);
}

ResizeConvertObjBatchingStage::ResizeConvertObjBatchingStage(std::shared_ptr<edk::ModelLoader> model,
                                                             uint32_t batchsize, int dev_id,
                                                             std::shared_ptr<RCOpResource> rcop_res)
    : ObjBatchingStage(model, batchsize), rcop_res_(rcop_res), dev_id_(dev_id) {}

ResizeConvertObjBatchingStage::~ResizeConvertObjBatchingStage() {}

std::shared_ptr<InferTask> ResizeConvertObjBatchingStage::Batching(std::shared_ptr<CNFrameInfo> finfo,
                                                                   std::shared_ptr<CNInferObject> obj) {
  CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(finfo->datas[CNDataFramePtrKey]);
  void* src_y = frame->data[0]->GetMutableMluData();
  void* src_uv = frame->data[1]->GetMutableMluData();
  QueuingTicket ticket = rcop_res_->PickUpTicket();
  std::shared_ptr<RCOpValue> value = rcop_res_->WaitResourceByTicket(&ticket);
  edk::MluResizeConvertOp::ColorMode cmode = edk::MluResizeConvertOp::ColorMode::YUV2ABGR_NV12;
  if (frame->fmt == CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV12) {
    cmode = edk::MluResizeConvertOp::ColorMode::YUV2RGBA_NV12;
  } else if (frame->fmt == CNDataFormat::CN_PIXEL_FORMAT_YUV420_NV21) {
    cmode = edk::MluResizeConvertOp::ColorMode::YUV2RGBA_NV21;
  } else {
    throw CnstreamError("Can not handle this frame with format :" + std::to_string(static_cast<int>(frame->fmt)));
  }
  if (!rcop_res_->Initialized()) {
    uint32_t dst_w = model_->InputShapes()[0].w;
    uint32_t dst_h = model_->InputShapes()[0].h;
    edk::MluContext mlu_ctx;
    mlu_ctx.SetDeviceId(dev_id_);
    mlu_ctx.ConfigureForThisThread();
    edk::CoreVersion core_ver = mlu_ctx.GetCoreVersion();
    rcop_res_->Init(dst_w, dst_h, cmode, core_ver);
  } else {
    edk::MluResizeConvertOp::Attr rc_attr = value->op.GetAttr();
    if (cmode != rc_attr.color_mode) {
      throw CnstreamError(
          "Resize convert operator should be reinitialized, but we can not do this."
          " Maybe you have different pixel format between each frame, wo can not use mlu preprocessing to deal with "
          "this.");
    }
  }
  edk::MluResizeConvertOp::InputData input_data;
  input_data.src_w = frame->width;
  input_data.src_h = frame->height;
  input_data.src_stride = frame->stride[0];
  input_data.planes[0] = src_y;
  input_data.planes[1] = src_uv;
  input_data.crop_x = obj->bbox.x * frame->width;
  input_data.crop_y = obj->bbox.y * frame->height;
  input_data.crop_w = obj->bbox.w * frame->width;
  input_data.crop_h = obj->bbox.h * frame->height;
  value->op.BatchingUp(input_data);
  rcop_res_->DeallingDone();
  return NULL;
}

ScalerObjBatchingStage::ScalerObjBatchingStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize,
                                               std::shared_ptr<MluInputResource> mlu_input_res)
    : IOObjBatchingStage(model, batchsize, mlu_input_res) {}

ScalerObjBatchingStage::~ScalerObjBatchingStage() {}

void ScalerObjBatchingStage::ProcessOneObject(std::shared_ptr<CNFrameInfo> finfo, std::shared_ptr<CNInferObject> obj,
                                              uint32_t batch_idx, const IOResValue& value) {
  CNDataFramePtr frame = cnstream::any_cast<CNDataFramePtr>(finfo->datas[CNDataFramePtrKey]);
  void* src_y = frame->data[0]->GetMutableMluData();
  void* src_uv = frame->data[1]->GetMutableMluData();
  void* dst = value.datas[0].Offset(batch_idx);
  cncodecWorkInfo work_info;
  cncodecFrame src_frame;
  cncodecFrame dst_frame;
  memset(&work_info, 0, sizeof(work_info));
  memset(&src_frame, 0, sizeof(src_frame));
  memset(&dst_frame, 0, sizeof(dst_frame));

  src_frame.pixelFmt = CNCODEC_PIX_FMT_NV21;
  src_frame.colorSpace = CNCODEC_COLOR_SPACE_BT_709;
  src_frame.width = frame->width;
  src_frame.height = frame->height;
  src_frame.planeNum = frame->GetPlanes();
  src_frame.plane[0].size = frame->GetPlaneBytes(0);
  src_frame.plane[0].addr = reinterpret_cast<u64_t>(src_y);
  src_frame.plane[1].size = frame->GetPlaneBytes(1);
  src_frame.plane[1].addr = reinterpret_cast<u64_t>(src_uv);
  src_frame.stride[0] = frame->stride[0];
  src_frame.stride[1] = frame->stride[1];
  src_frame.channel = 1;
  src_frame.deviceId = 0;  // FIXME

  const auto sp = value.datas[0].shape;  // model input shape
  auto align_to_128 = [](uint32_t x) { return (x + 127) & ~127; };
  auto row_align = align_to_128(sp.w * 4);
  dst_frame.width = sp.w;
  dst_frame.height = sp.h;
  dst_frame.pixelFmt = CNCODEC_PIX_FMT_ARGB;
  dst_frame.planeNum = 1;
  dst_frame.plane[0].size = row_align * sp.h;
  dst_frame.stride[0] = row_align;
  dst_frame.plane[0].addr = reinterpret_cast<u64_t>(dst);

  work_info.inMsg.instance = 0;

  cncodecRectangle roi;
  roi.left = obj->bbox.x * src_frame.width;
  roi.top = obj->bbox.y * src_frame.height;
  roi.right = (obj->bbox.x + obj->bbox.w) * src_frame.width;
  roi.bottom = (obj->bbox.y + obj->bbox.h) * src_frame.height;

  auto ret = cncodecImageTransform(&dst_frame, &roi, &src_frame, nullptr, CNCODEC_Filter_BiLinear, &work_info);

  if (CNCODEC_SUCCESS != ret) {
    throw CnstreamError("scaler failed, error code:" + std::to_string(ret));
  }
}

}  // namespace cnstream
