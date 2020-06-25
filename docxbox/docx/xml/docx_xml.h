// Copyright (c) 2020 gyselroth GmbH
// Licensed under the MIT License - https://opensource.org/licenses/MIT

#ifndef DOCXBOX_DOCX_XML_DOCX_XML_H_
#define DOCXBOX_DOCX_XML_DOCX_XML_H_

#include <docxbox/helper/helper_file.h>
#include <docxbox/helper/helper_string.h>
#include <docxbox/helper/helper_xml.h>

#include <vendor/tinyxml2/tinyxml2.h>

#include <iostream>
#include <string>
#include <vector>

class docx_xml {
 public:
  docx_xml(int argc, const std::vector<std::string>& argv);

  static bool IsXmlFileContainingText(const std::string &filename);

 protected:
  int argc_;
  std::vector<std::string> argv_;

  std::string document_text_;
  std::string path_xml_file_;
  std::string xml_;

  tinyxml2::XMLElement *GetBodyByComponentPath(
      tinyxml2::XMLDocument &doc, const std::string& path_xml) const;

  bool SaveXml(bool compress);

  static void RemoveTmpEndingFromDocxPath(std::string &xml_filename);
};

#endif  // DOCXBOX_DOCX_XML_DOCX_XML_H_
