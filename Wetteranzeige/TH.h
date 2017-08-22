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

#ifndef _TH_H_
#define _TH_H_
/**
 * Temperature and humidity.
 */
class TH
{
public:
  bool dataValid = false;
  float temperature = 0;
  float humidity = 0;
  float vaporPressure = 0;
};
#endif

