//
// Created by arnob on 7/14/20.
//

#ifndef FACERECOGNITION_DB_H
#define FACERECOGNITION_DB_H

#include "db_item.h"

#define DB_NULL "NONE"
// #define DB_DIR "face_info/"

struct db_item get_face_information(const char *face_id);


#endif //FACERECOGNITION_DB_H
