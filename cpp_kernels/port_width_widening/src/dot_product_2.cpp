/**
* Copyright (C) 2020 Xilinx, Inc
*
* Licensed under the Apache License, Version 2.0 (the "License"). You may
* not use this file except in compliance with the License. A copy of the
* License is located at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
* License for the specific language governing permissions and limitations
* under the License.
*/

#include <stdint.h>
#define DATA_WIDTH 16

extern "C" {
void dot_product_2(const uint32_t* a, const uint32_t* b, uint64_t* res, const int size, const int reps) {
// In this case it is known before kernel compilation that operations
// on the 64 bit bundle will take place at least 16 times, therefore
// the port width is by default set as the maximum default value of 512.

loop_reps:
    for (int i = 0; i < reps; i++) {
    dot_product_outer:
        for (int j = 0; j < size; j += DATA_WIDTH) {
        dot_product_inner:
            for (int k = 0; k < DATA_WIDTH; k++) {
                res[j + k] = a[j + k] * b[j + k];
            }
        }
    }
}
}
