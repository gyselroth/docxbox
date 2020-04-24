// Copyright (c) 2020 gyselroth GmbH

#include <docxbox/docx/docx_archive.h>
#include <docxbox/ext/ext_miniz_cpp.hpp>

docx_archive::docx_archive(int argc, char **argv) {
  argc_ = argc;
  argv_ = argv;

  path_working_directory_ = getenv("PWD");
}

// Setup path to DOCX file,
// absolute or relative from execution path, from given argument
bool docx_archive::InitPathDocxByArgV(int index_path_argument) {
  path_docx_in_ = docxbox::AppArguments::ResolvePathFromArgument(
      path_working_directory_,
      argc_,
      argv_,
      index_path_argument);

  if (!helper::File::FileExists(path_docx_in_))
    throw "Error - File not found: " + path_docx_in_ + "\n";

  return true;
}

// Output paths of files (and directories) within DOCX file
bool docx_archive::ListFiles(bool as_json, bool images_only) {
  if (!docxbox::AppArguments::IsArgumentGiven(argc_, 2, "DOCX filename"))
    return false;

  try {
    InitPathDocxByArgV(3);
  } catch (std::string &message) {
    std::cerr << message;

    return false;
  }

  miniz_cpp::zip_file docx_file(path_docx_in_);

  if (as_json)
    miniz_cpp_ext::PrintDir(docx_file, true, images_only);
  else
    docx_file.printdir();

  return true;
}

// List contained images and their attributes
bool docx_archive::ListImages(bool as_json) {
  if (as_json) return ListFiles(as_json, true);

  if (!UnzipDocx("-" + helper::File::GetTmpName())) return false;

  miniz_cpp::zip_file docx_file(path_docx_in_);

  miniz_cpp_ext::PrintDir(docx_file, false, true);

  return miniz_cpp_ext::RemoveExtract(path_extract_, docx_file.infolist());
}

// Render path (string) where to extract given DOCX file
void docx_archive::InitExtractionPath(const std::string &directory_appendix,
                                      const std::string &path_docx) {
  path_extract_ =
      helper::File::GetLastPathSegment(path_docx)
          + (directory_appendix.empty()
             ? "-extracted"
             : directory_appendix);
}

// Unzip all files of DOCX file
bool docx_archive::UnzipDocx(const std::string &directory_appendix,
                             bool ensure_is_docx,
                             bool format_xml_files) {
  if (!docxbox::AppArguments::IsArgumentGiven(
      argc_,
      2,
      "Filename of DOCX to be extracted"))
    return false;

  try {
    InitPathDocxByArgV(3);
  } catch (std::string &message) {
    std::cerr << message << "\n";

    return false;
  }

  InitExtractionPath(directory_appendix, path_docx_in_);

  miniz_cpp::zip_file docx_file(path_docx_in_);

  auto file_list = docx_file.infolist();

  mkdir(path_extract_.c_str(), 0777);
  miniz_cpp_ext::CreateSubDirectories(path_extract_, file_list);

  docx_file.extractall(path_working_directory_ + "/" + path_extract_);

  if (ensure_is_docx && IsDocx()) {
    std::cerr << "Error: " << path_docx_in_ << " is not a DOCX document.\n";

    return false;
  }

  return format_xml_files
    ? miniz_cpp_ext::IndentXmlFiles(path_extract_, file_list)
    : true;
}

// Check formal structure of DOCX archive - mandatory files given?
bool docx_archive::IsDocx() {
  return
      helper::File::IsDirectory(path_extract_ + "/rels")
          && helper::File::IsDirectory(path_extract_ + "/docProps")
          && helper::File::IsDirectory(path_extract_ + "/word")
          && helper::File::FileExists(path_extract_ + "/[Content_Types].xml")
          && helper::File::FileExists(path_extract_ + "/_rels/.rels")
          && helper::File::FileExists(path_extract_ + "/docProps/app.xml")
          && helper::File::FileExists(path_extract_ + "/docProps/core.xml")
          && helper::File::FileExists(path_extract_ + "/word/document.xml");
}

// Unzip all (than remove everything but) media files from DOCX file
bool docx_archive::UnzipMedia() {
  if (!UnzipDocx("-media")) return false;

  miniz_cpp::zip_file docx_file(path_docx_in_);

  auto file_list = docx_file.infolist();

  miniz_cpp_ext::ReduceExtractToImages(path_extract_, file_list);

  return true;
}

// Output meta data from within given DOCX file:
// Creation date, revision, title, language, used fonts, contained media files
bool docx_archive::ListMeta(bool as_json) {
  if (!UnzipDocx("-" + helper::File::GetTmpName())) return false;

  miniz_cpp::zip_file docx_file(path_docx_in_);

  auto file_list = docx_file.infolist();

  auto *meta = new docx_meta(argc_, argv_);
  meta->SetPathExtract(path_extract_);

  if (as_json) meta->SetOutputAsJson(true);

  int index_app = 0;
  int index_core = 0;

  for (const auto &file_in_zip : file_list) {
    const char *path_file_within_docx = file_in_zip.filename.c_str();

    std::string
        path_file_absolute = path_extract_ + "/" + path_file_within_docx;

    // Extract meta data from app.xml and core.xml,
    // data is output when both are collected
    // or another of the same kind is found (e.g. merged documents)
    if (helper::String::EndsWith(file_in_zip.filename, "app.xml")) {
      meta->CollectFromAppXml(
          file_in_zip.filename,
          helper::File::GetFileContents(path_file_absolute));

      ++index_app;
    } else if (helper::String::EndsWith(file_in_zip.filename, "core.xml")) {
      meta->LoadCoreXml(path_file_absolute);
      meta->CollectFromCoreXml(file_in_zip.filename);

      ++index_core;
    }
  }

  // Output anything that hasn't been yet
  meta->Output();

  miniz_cpp_ext::RemoveExtract(path_extract_, file_list);

  delete meta;

  return true;
}

bool docx_archive::ModifyMeta() {
  auto *meta = new docx_meta(argc_, argv_);

  if (!meta->InitModificationArguments()
      || !UnzipDocx("-" + helper::File::GetTmpName())) {
    std::cerr << "Initialization for meta modification failed.\n";

    delete meta;

    return false;
  }

  miniz_cpp::zip_file docx_file(path_docx_in_);
  auto file_list = docx_file.infolist();

  meta->SetPathExtract(path_extract_);

  if (!meta->UpsertAttribute()) {
    std::cerr << "Update/Insert meta attribute failed.\n";

    delete meta;

    return false;
  }

  // Modifiable meta attributes are in docProps/core.xml
  try {
    meta->SaveCoreXml();
  } catch (std::string &message) {
    std::cerr << message;
  }

  delete meta;

  std::string path_docx_out;

  if (argc_ >= 6) {
    // Result filename is given as argument
    path_docx_out =
        helper::File::ResolvePath(path_working_directory_, argv_[5]);
  } else {
    // Overwrite original DOCX
    path_docx_out = path_docx_in_;
  }

  auto attribute = meta->GetAttribute();

  if (!Zip(path_extract_, path_docx_out + "tmp",
      attribute != docx_meta::Attribute_Created,
      attribute != docx_meta::Attribute_Modified)) {
    std::cerr << "DOCX creation failed.\n";

    return false;
  }

  if (argc_ < 6) helper::File::Remove(path_docx_in_.c_str());

  std::rename(
      std::string(path_docx_out).append("tmp").c_str(),
      path_docx_out.c_str());

  return miniz_cpp_ext::RemoveExtract(path_extract_, file_list);
}

// List referenced fonts and their metrics
bool docx_archive::ListFonts(bool as_json) {
  if (!UnzipDocx("-" + helper::File::GetTmpName())) return false;

  miniz_cpp::zip_file docx_file(path_docx_in_);

  auto file_list = docx_file.infolist();

  auto *fontTable = new docx_fontTable();

  if (as_json) std::cout << "[";

  int index_font = 0;

  for (const auto &file_in_zip : file_list) {
    if (helper::String::EndsWith(file_in_zip.filename, "fontTable.xml")) {
      // Extract fonts data from all fontTable.xml files
      const char *path_file_within_docx = file_in_zip.filename.c_str();

      std::string path_file_absolute =
          path_extract_ + "/" + path_file_within_docx;

      fontTable->CollectFontsMetrics(
          helper::File::GetFileContents(path_file_absolute));

      if (as_json) {
        if (index_font > 0) std::cout << ",";

        fontTable->OutputAsJson(file_in_zip.filename);
      } else {
        fontTable->OutputPlain(file_in_zip.filename);
      }

      fontTable->Clear();

      ++index_font;
    }
  }

  delete fontTable;

  if (as_json) std::cout << "]";

  miniz_cpp_ext::RemoveExtract(path_extract_, file_list);

  return true;
}

bool docx_archive::GetText(bool newline_at_segments) {
  if (!UnzipDocx("-" + helper::File::GetTmpName())) return false;

  miniz_cpp::zip_file docx_file(path_docx_in_);

  auto file_list = docx_file.infolist();

  auto parser = new docx_xml_to_plaintext(argc_, argv_);

  for (const auto &file_in_zip : file_list) {
    // TODO(kay): fetch from all textual XML files, instead only document.xml

    if (!helper::String::EndsWith(file_in_zip.filename, "word/document.xml"))
      continue;

    parser->GetTextFromXmlFile(
        path_extract_ + "/" + file_in_zip.filename,
        newline_at_segments);
  }

  parser->Output();

  delete parser;

  miniz_cpp_ext::RemoveExtract(path_extract_, file_list);

  return true;
}

bool docx_archive::ListMergeFields(bool as_json) {
  if (!UnzipDocx("-" + helper::File::GetTmpName())) return false;

  miniz_cpp::zip_file docx_file(path_docx_in_);

  auto file_list = docx_file.infolist();

  auto parser = new docx_xml_fields(argc_, argv_);

  for (const auto &file_in_zip : file_list) {
    // TODO(kay): fetch from all textual XML files, instead only document.xml

    if (!helper::String::EndsWith(file_in_zip.filename, "word/document.xml"))
      continue;

    parser->CollectMergeFields(path_extract_ + "/" + file_in_zip.filename);
  }

  parser->Output(as_json);

  delete parser;

  miniz_cpp_ext::RemoveExtract(path_extract_, file_list);

  return true;
}

bool docx_archive::ReplaceImage() {
  if (
      !docxbox::AppArguments::IsArgumentGiven(
          argc_,
          2,
          "Filename of DOCX")
          || !docxbox::AppArguments::IsArgumentGiven(
          argc_,
          3,
          "Filename of image to be replaced")
          || !docxbox::AppArguments::IsArgumentGiven(
              argc_,
              4,
              "Filename of replacement image")
      )
    return false;

  if (!UnzipDocx("-" + helper::File::GetTmpName())) return false;

  std::string image_original = argv_[3];

  miniz_cpp::zip_file docx_file(path_docx_in_);

  auto file_list = docx_file.infolist();

  try {
    bool found = false;

    for (const auto &file_in_zip : file_list) {
      if (!helper::String::EndsWith(file_in_zip.filename, image_original))
        continue;

      found = true;

      std::string
          path_image_original = path_extract_ + "/" + file_in_zip.filename;

      if (!helper::File::Remove(path_image_original.c_str()))
        throw "Failed replace " + image_original + "\n";

      std::string path_image_replacement =
          helper::File::ResolvePath(path_working_directory_, argv_[4]);

      helper::File::CopyFile(path_image_original, path_image_replacement);

      break;
    }

    if (!found)
      throw "Cannot replace " + image_original
            + " - no such image within " + path_docx_in_ + "\n";

    std::string path_docx_out =
        argc_ >= 6
        // Result filename is given as argument
        ? helper::File::ResolvePath(
            path_working_directory_,
            argv_[5])
        // Overwrite original DOCX
        : path_docx_in_;

    if (!Zip(path_extract_, path_docx_out + "tmp"))
      throw "DOCX creation failed.\n";

    if (argc_ < 6) helper::File::Remove(path_docx_in_.c_str());

    std::rename(
        std::string(path_docx_out).append("tmp").c_str(),
        path_docx_out.c_str());
  } catch (std::string &message) {
    std::cerr << message;

    miniz_cpp_ext::RemoveExtract(path_extract_, file_list);

    return false;
  }

  return miniz_cpp_ext::RemoveExtract(path_extract_, file_list);
}

bool docx_archive::ReplaceText() {
  if (!docxbox::AppArguments::IsArgumentGiven(argc_, 2, "DOCX Filename")
      || !docxbox::AppArguments::IsArgumentGiven(
          argc_,
          3,
          "String to be found (and replaced)")
      || !docxbox::AppArguments::IsArgumentGiven(
          argc_,
          4,
          "Replacement")) return false;

  std::string search = argv_[3];
  std::string replacement = argv_[4];

  if (!UnzipDocx("-" + helper::File::GetTmpName())) return false;

  miniz_cpp::zip_file docx_file(path_docx_in_);

  auto file_list = docx_file.infolist();

  auto parser = new docx_xml_replace(argc_, argv_);

  for (const auto &file_in_zip : file_list) {
    if (!docx_xml::IsXmlFileContainingText(file_in_zip.filename)) continue;

    std::string path_file_absolute = path_extract_ + "/" + file_in_zip.filename;

    if (!parser->ReplaceInXml(path_file_absolute, search, replacement)) {
      std::cerr << "Error: Failed replace string in: "
                << file_in_zip.filename << "\n";

      delete parser;

      return false;
    }
  }

  delete parser;

  std::string path_docx_out =
      argc_ >= 6
      // Result filename is given as argument
      ? helper::File::ResolvePath(
          path_working_directory_,
          argv_[5])
      // Overwrite original DOCX
      : path_docx_in_;

  if (!Zip(path_extract_, path_docx_out + "tmp")) {
    std::cerr << "DOCX creation failed.\n";

    return false;
  }

  if (argc_ < 6) helper::File::Remove(path_docx_in_.c_str());

  std::rename(
      std::string(path_docx_out).append("tmp").c_str(),
      path_docx_out.c_str());

  return miniz_cpp_ext::RemoveExtract(path_extract_, file_list);
}

bool docx_archive::RemoveBetweenText() {
  if (!docxbox::AppArguments::IsArgumentGiven(argc_, 2, "DOCX Filename")
      || !docxbox::AppArguments::IsArgumentGiven(
          argc_,
          3,
          "String left-hand-side of part to be removed")
      || !docxbox::AppArguments::IsArgumentGiven(
          argc_,
          4,
          "String right-hand-side of part to be removed")) return false;

  std::string lhs = argv_[3];
  std::string rhs = argv_[4];

  if (!UnzipDocx("-" + helper::File::GetTmpName())) return false;

  miniz_cpp::zip_file docx_file(path_docx_in_);

  auto file_list = docx_file.infolist();

  auto parser = new docx_xml_remove(argc_, argv_);

  for (const auto &file_in_zip : file_list) {
    if (!docx_xml::IsXmlFileContainingText(file_in_zip.filename)) continue;

    std::string path_file_absolute = path_extract_ + "/" + file_in_zip.filename;

    if (!parser->RemoveBetweenStringsInXml(path_file_absolute, lhs, rhs)) {
      std::cerr << "Error: Failed to remove content from: "
                << file_in_zip.filename << "\n";

      delete parser;

      return false;
    }
  }

  delete parser;

  std::string path_docx_out =
      argc_ >= 7
      // Result filename is given as argument
      ? helper::File::ResolvePath(
          path_working_directory_,
          argv_[6])
      // Overwrite original DOCX
      : path_docx_in_;

  if (!Zip(path_extract_, path_docx_out + "tmp")) {
    std::cerr << "DOCX creation failed.\n";

    return false;
  }

  if (argc_ < 7) helper::File::Remove(path_docx_in_.c_str());

  std::rename(
      std::string(path_docx_out).append("tmp").c_str(),
      path_docx_out.c_str());

  return miniz_cpp_ext::RemoveExtract(path_extract_, file_list);
}

bool docx_archive::ReplaceAllTextByLoremIpsum() {
  if (!UnzipDocx("-" + helper::File::GetTmpName())) return false;

  miniz_cpp::zip_file docx_file(path_docx_in_);

  auto file_list = docx_file.infolist();

  auto parser = new docx_xml_lorem(argc_, argv_);

  for (const auto &file_in_zip : file_list) {
    if (!docx_xml::IsXmlFileContainingText(file_in_zip.filename)) continue;

    std::string path_file_absolute = path_extract_ + "/" + file_in_zip.filename;

    if (!parser->RandomizeAllTextInXml(path_file_absolute)) {
      std::cerr << "Error: Failed insert lorem ipsum in: "
                << file_in_zip.filename << "\n";

      delete parser;

      return false;
    }
  }

  delete parser;

  std::string path_docx_out =
      argc_ >= 4
      // Result filename is given as argument
      ? helper::File::ResolvePath(path_working_directory_, argv_[3])
      // Overwrite original DOCX
      : path_docx_in_;

  if (!Zip(path_extract_, path_docx_out + "tmp")) {
    std::cerr << "DOCX creation failed.\n";

    return false;
  }

  if (argc_ < 6) helper::File::Remove(path_docx_in_.c_str());

  std::rename(
      std::string(path_docx_out).append("tmp").c_str(),
      path_docx_out.c_str());

  return miniz_cpp_ext::RemoveExtract(path_extract_, file_list);
}

// Zip files into given path into DOCX of given filename
// Optionally update "creation" and "modified" meta attributes (core.xml)
// to current date-time value
bool docx_archive::Zip(
    std::string path_directory,
    std::string path_docx_result,
    bool update_created,
    bool update_modified) {
  if (path_directory.empty()) {
    if (argc_ <= 2) {
      std::cerr << "Missing argument: path of directory to be zipped\n";

      return false;
    }

    if (argc_ <= 3) {
      std::cerr << "Missing argument: filename of docx to be created\n";

      return false;
    }

    path_directory =
        helper::File::ResolvePath(path_working_directory_, argv_[2]);

    path_docx_result =
        helper::File::ResolvePath(path_working_directory_, argv_[3], false);
  } else {
    if (!helper::File::IsDirectory(path_directory)) {
      std::cerr << "Not a directory: " << path_directory;

      return false;
    }
  }

  if (update_created)
    UpdateCoreXmlDate(docx_meta::Attribute::Attribute_Created);

  if (update_modified)
    UpdateCoreXmlDate(docx_meta::Attribute::Attribute_Modified);

  // Get relative paths of all files to be zipped
  std::vector<std::string> files_in_zip;

  files_in_zip = helper::File::ScanDirRecursive(
      path_directory.c_str(),
      files_in_zip,
      path_directory + "/");

  miniz_cpp::zip_file file;

  for (const auto &file_in_zip : files_in_zip) {
    std::string path_file_absolute =
        std::string(path_directory + "/").append(file_in_zip);

    file.writestr(
        file_in_zip,
        helper::File::GetFileContents(path_file_absolute));
  }

  file.save(path_docx_result);

  return helper::File::FileExists(path_docx_result);
}

// Update given meta date attribute and immediately save updated core.xml
// TODO(kay): add multi-attributes variation of method
//  to load/save only once than
bool docx_archive::UpdateCoreXmlDate(docx_meta::Attribute attribute) {
  // TODO(kay): add gate - ensure attribute is a date

  auto meta = new docx_meta(argc_, argv_);

  meta->SetPathExtract(path_extract_);

  meta->SetAttribute(attribute);
  meta->SetValue(helper::DateTime::GetCurrentDateTimeInIso8601());

  auto result = meta->UpsertAttribute(true);

  delete meta;

  return result;
}
