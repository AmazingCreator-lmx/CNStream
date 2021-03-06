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

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#ifdef HAVE_OPENCV
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#if (CV_MAJOR_VERSION >= 3)
#include "opencv2/imgcodecs/imgcodecs.hpp"
#endif
#else
#error OpenCV required
#endif
#include "cnstream_frame_va.hpp"
#include "cnstream_module.hpp"
#include "osd.hpp"
#include "test_base.hpp"

namespace cnstream {

static constexpr const char *gname = "osd";
static constexpr const char *glabel_path = "../../modules/unitest/osd/test_label.txt";

TEST(Osd, Construct) {
  std::shared_ptr<Module> osd = std::make_shared<Osd>(gname);
  EXPECT_STREQ(osd->GetName().c_str(), gname);
}

TEST(Osd, OpenClose) {
  std::shared_ptr<Module> osd = std::make_shared<Osd>(gname);
  ModuleParamSet param;
  EXPECT_TRUE(osd->Open(param));
  param["label_path"] = "test-osd";
  EXPECT_TRUE(osd->Open(param)) << "if can not read labels, function should not return false";
  std::string label_path = GetExePath() + glabel_path;
  param["label_path"] = label_path;
  EXPECT_TRUE(osd->Open(param));
  osd->Close();
}

TEST(Osd, Process) {
  // create osd
  std::shared_ptr<Module> osd = std::make_shared<Osd>(gname);
  ModuleParamSet param;
  std::string label_path = GetExePath() + glabel_path;
  param["label_path"] = label_path;
  ASSERT_TRUE(osd->Open(param));

  // prepare data
  int width = 1920;
  int height = 1080;
  cv::Mat img(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
  auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
  data->SetStreamIndex(0);
  frame->frame_id = 1;
  data->timestamp = 1000;
  frame->width = width;
  frame->height = height;
  frame->ptr_cpu[0] = img.data;
  frame->stride[0] = width;
  frame->ctx.dev_type = DevContext::DevType::CPU;
  frame->fmt = CN_PIXEL_FORMAT_BGR24;
  frame->CopyToSyncMem();
  data->datas[CNDataFramePtrKey] = frame;

  CNObjsVec objs;
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(11);
  CNInferBoundingBox bbox = {0.6, 0.4, 0.6, 0.3};
  obj->bbox = bbox;
  objs.push_back(obj);

  auto obj2 = std::make_shared<CNInferObject>();
  obj2->id = std::to_string(12);
  bbox = {0.1, -0.2, 0.3, 0.4};
  obj2->bbox = bbox;
  objs.push_back(obj2);

  for (int i = 0; i < 5; ++i) {
    auto obj = std::make_shared<CNInferObject>();
    obj->id = std::to_string(i);
    float val = i * 0.1;
    CNInferBoundingBox bbox = {val, val, val, val};
    obj->bbox = bbox;
    objs.push_back(obj);
  }

  data->datas[cnstream::CNObjsVecKey] = objs;
  EXPECT_EQ(osd->Process(data), 0);
}

TEST(Osd, ProcessSecondary) {
  // create osd
  std::shared_ptr<Module> osd = std::make_shared<Osd>(gname);
  ModuleParamSet param;
  std::string label_path = GetExePath() + glabel_path;
  param["label_path"] = label_path;
  param["secodary_labtel_path"] = label_path;
  param["attr_key"] = "classifaction";
  ASSERT_TRUE(osd->Open(param));

  // prepare data
  int width = 1920;
  int height = 1080;
  cv::Mat img(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
  auto data = cnstream::CNFrameInfo::Create(std::to_string(0));
  std::shared_ptr<CNDataFrame> frame(new (std::nothrow) CNDataFrame());
  data->SetStreamIndex(0);
  frame->frame_id = 1;
  data->timestamp = 1000;
  frame->width = width;
  frame->height = height;
  frame->ptr_cpu[0] = img.data;
  frame->stride[0] = width;
  frame->ctx.dev_type = DevContext::DevType::CPU;
  frame->fmt = CN_PIXEL_FORMAT_BGR24;
  frame->CopyToSyncMem();
  data->datas[CNDataFramePtrKey] = frame;



  CNObjsVec objs;
  auto obj = std::make_shared<CNInferObject>();
  obj->id = std::to_string(11);
  CNInferBoundingBox bbox = {0.6, 0.4, 0.6, 0.3};
  obj->bbox = bbox;
  cnstream::CNInferAttr attr;
  attr.id = 0;
  attr.value = -1;
  attr.score = -1;
  obj->AddAttribute("classifaction", attr);
  objs.push_back(obj);



  auto obj2 = std::make_shared<CNInferObject>();
  obj2->id = std::to_string(12);
  bbox = {0.1, -0.2, 0.3, 0.4};
  obj2->bbox = bbox;
  cnstream::CNInferAttr attr2;
  attr2.id = 0;
  attr2.value = 2;
  attr2.score = 0.6;
  obj2->AddAttribute("classifaction", attr2);
  objs.push_back(obj2);

  for (int i = 0; i < 5; ++i) {
    auto obj = std::make_shared<CNInferObject>();
    obj->id = std::to_string(i);
    float val = i * 0.1;
    CNInferBoundingBox bbox = {val, val, val, val};
    obj->bbox = bbox;
    objs.push_back(obj);
  }

  data->datas[cnstream::CNObjsVecKey] = objs;
  EXPECT_EQ(osd->Process(data), 0);
}

TEST(Osd, CheckParamSet) {
  std::shared_ptr<Module> osd = std::make_shared<Osd>(gname);
  ModuleParamSet param;
  param.clear();
  EXPECT_TRUE(osd->CheckParamSet(param));
  param.insert(std::make_pair("label_path", "a"));
  EXPECT_FALSE(osd->CheckParamSet(param));
  param.clear();
  std::string label_path = GetExePath() + glabel_path;
  param["label_path"] = label_path;
  param.insert(std::make_pair("chinese_label_flag", "a"));
  EXPECT_FALSE(osd->CheckParamSet(param));
  param["chinese_label_flag"] = "true";
  EXPECT_TRUE(osd->CheckParamSet(param));
}

}  // namespace cnstream
