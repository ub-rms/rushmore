//
// Created by arnob on 7/10/20.
//

#ifndef FACERECOGNITION_TA_H
#define FACERECOGNITION_TA_H

#define RENDER_IMAGE 0x00000001
#define GET_INFO_AND_RENDER_IMAGE 0x00000002

#include "../rushmore_dummy/tee_api.h"


bool ta_render_image(struct tee_arg *arg);

bool ta_get_info_and_render_image(struct tee_arg *arg);

bool ta_entry(struct tee_arg *arg);


#endif //FACERECOGNITION_TA_H
