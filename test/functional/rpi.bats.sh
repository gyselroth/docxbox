#!/usr/bin/env bats

# Copyright (c) 2020 gyselroth GmbH
# Licensed under the MIT License - https://opensource.org/licenses/MIT

load _helper

DOCXBOX_BINARY="$BATS_TEST_DIRNAME/../tmp/docxbox"
path_docx="test/tmp/cp_table_unordered_list_images.docx"
path_extracted_image="test/tmp/unziped/word/media/image2.jpeg"
path_jpeg="test/assets/images/2100x400.jpeg"


base_command="docxbox rpi filename.docx"

@test "Output of \"docxbox rpi {missing filename}\" is an error message" {
  run "${DOCXBOX_BINARY}" rpi
  [ "$status" -ne 0 ]
  [ "docxBox Error - Missing argument: DOCX filename" = "${lines[0]}" ]
}

@test "Output of \"${base_command} {missing argument}\" is an error message" {
  run "${DOCXBOX_BINARY}" rpi "${path_docx}"
  [ "$status" -ne 0 ]
  [ "docxBox Error - Missing argument: Filename of image to be replaced" = "${lines[0]}" ]
}

missing_argument="imageName {missingReplacementImageName}"
@test "Output of \"${base_command} ${missing_argument}\" is an error message" {
  run "${DOCXBOX_BINARY}" rpi "${path_docx}" image2.jpeg
  [ "$status" -ne 0 ]
  [ "docxBox Error - Missing argument: Filename of replacement image" = "${lines[0]}" ]
}

appendix="an image can be replaced"
@test "With \"${base_command} imageName replacementImageName\" ${appendix}" {

  run "${DOCXBOX_BINARY}" rpi "${path_docx}" image2.jpeg "${path_jpeg}"
  [ "$status" -eq 0 ]
  if [ ! -d test/tmp/unziped ]; then
    mkdir test/tmp/unziped;
    unzip "${path_docx}" -d test/tmp/unziped;
  fi

  file "${path_extracted_image}" | grep --count "2100x400"
}

arguments="imageName replacementImageName newFilename.docx"
appendix_new_docx="an image can be replaced and saved to new doxc"
@test "With \"${base_command} ${arguments}\" ${appendix_new_docx}" {
  path_docx_out="test/tmp/newImage.docx"

  run "${DOCXBOX_BINARY}" rpi "${path_docx}" image2.jpeg "${path_jpeg}" "${path_docx_out}"
  [ "$status" -eq 0 ]
  if [ ! -d test/tmp/unziped ]; then
    mkdir test/tmp/unziped;
    unzip test/tmp/newImage.docx -d test/tmp/unziped;
  fi

  file "${path_extracted_image}" | grep --count "2100x400"
}

@test "Output of \"${base_command} nonexistent.image\" is an error message" {
  err_log="test/tmp/err.log"
  error_message="docxBox Error - Copy file failed - file not found:"

  "${DOCXBOX_BINARY}" rpi "${path_docx}" image2.jpeg nonexistent.jpeg 2>&1 | tee "${err_log}"
  cat "${err_log}" | grep --count "${error_message}"
}

@test "Output of \"docxbox rpi nonexistent.docx\" is an error message" {
  err_log="test/tmp/err.log"

  run "${DOCXBOX_BINARY}" rpi nonexistent.docx
  [ "$status" -ne 0 ]

  "${DOCXBOX_BINARY}" rpi nonexistent.docx 2>&1 image2.jpeg "${path_jpeg}" | tee "${err_log}"
  cat "${err_log}" | grep --count "docxBox Error - File not found:"
}

@test "Output of \"docxbox rpi wrong_file_type\" is an error message" {
  pattern="docxBox Error - File is no ZIP archive:"
  err_log="test/tmp/err.log"
  wrong_file_types=(
  "test/tmp/cp_lorem_ipsum.pdf"
  "test/tmp/cp_mock_csv.csv"
  "test/tmp/cp_mock_excel.xls")

  for i in "${wrong_file_types[@]}"
  do
    "${DOCXBOX_BINARY}" rpi "${i}" image2.jpeg "${path_jpeg}" 2>&1 | tee "${err_log}"
    cat "${err_log}" | grep --count "${pattern}"
  done
}
