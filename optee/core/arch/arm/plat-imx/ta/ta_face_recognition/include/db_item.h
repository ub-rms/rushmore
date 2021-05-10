//
// Created by arnob on 7/16/20.
//

#ifndef FACERECOGNITION_DB_ITEM_H
#define FACERECOGNITION_DB_ITEM_H

#include <stdint.h>

struct db_item {
    uint16_t face_id;
    const char *face_information;
};

#endif //FACERECOGNITION_DB_ITEM_H
