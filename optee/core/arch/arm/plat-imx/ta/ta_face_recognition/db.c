//
// Created by arnob on 7/14/20.
//

#include <string.h>

#include "db.h"
#include "db_hardcoded_data.h"

struct db_item get_face_information(const char *face_id) {

    for (int i = 0; i < FACE_LIST_LENGTH; i++) {
        if (face_id == FACE_ITEM_LIST[i].face_id) {
            return FACE_ITEM_LIST[i];
        }
    }
    return (struct db_item) {face_id, DB_NULL};
}
