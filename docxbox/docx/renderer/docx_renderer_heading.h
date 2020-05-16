// Copyright (c) 2020 gyselroth GmbH
// Licensed under the MIT License - https://opensource.org/licenses/MIT

#ifndef DOCXBOX_DOCX_RENDERER_DOCX_RENDERER_HEADING_H_
#define DOCXBOX_DOCX_RENDERER_DOCX_RENDERER_HEADING_H_

#include <docxbox/docx/renderer/docx_renderer.h>
#include <docxbox/helper/helper_string.h>

#include <string>
#include <vector>

#include <vendor/json/single_include/nlohmann/json.hpp>

class docx_renderer_heading: docx_renderer {
 public:
  // Constructor
  explicit docx_renderer_heading(
      int argc, char **argv, const std::string &json);

  void SetLevel(int level);

  std::string Render();

 private:
  int level_;
  std::string text_;

  void InitSpecsFromJson();
};

#endif  // DOCXBOX_DOCX_RENDERER_DOCX_RENDERER_HEADING_H_