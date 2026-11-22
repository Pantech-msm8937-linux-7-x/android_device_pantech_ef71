/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * netmgrd: take the rtnetlink path of the rmnet driver on the mainline kernel.
 *
 * netmgrd picks how it creates the rmnet_data devices over rmnet0 from the
 * first characters of the kernel release (libdsutils ds_get_kernel_version):
 * only "4.9" and up of a 4.x kernel or a 5.x one use rtnetlink (type rmnet),
 * anything else the downstream rmnet_data netlink family, which a mainline
 * kernel does not have, so a 7.x kernel ends with no data link at all. The
 * upstream rmnet driver is the one the stock 4.19 kernel uses that way:
 * report its release.
 */

#include <string.h>

int ds_get_kernel_version(char *buf, unsigned int len)
{
    static const char release[] = "4.19";

    if (!buf || len < 2)
        return -1;

    memset(buf, 0, len);
    strncpy(buf, release, len - 1);
    return 0;
}
