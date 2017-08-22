/*
 * Copyright (C) 2017 Lakoja on github.com
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
 */
 
#ifndef _RECT_H_
#define _RECT_H_

#define SHRT_MIN -32768
#define SHRT_MAX 32767

class Rect 
{
public:
  int16_t Xstart;
  int16_t Xend;
  int16_t Ystart;
  int16_t Yend;

  Rect() {
    reset();
  }

  void reset()
  {
    Xstart = SHRT_MAX;
    Xend = SHRT_MIN;
    Ystart = SHRT_MAX;
    Yend = SHRT_MIN;
  }

  void addPixel(int16_t x, int16_t y)
  {
    if (x > Xend)
      Xend = x;
    if (x < Xstart)
      Xstart = x;

    if (y > Yend)
      Yend = y;
    if (y < Ystart)
      Ystart = y;
  }

  bool hasData()
  {
    return Xend > SHRT_MIN && Yend > SHRT_MIN;
  }

  int16_t getWidth()
  {
    if (Xend == SHRT_MIN)
      return 0;

    return Xend - Xstart + 1;
  }

  int16_t getHeight()
  {
    if (Yend == SHRT_MIN)
      return 0;

    return Yend - Ystart + 1;
  }
};

#endif
