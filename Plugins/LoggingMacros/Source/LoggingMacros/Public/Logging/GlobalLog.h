/*
 * Copyright (c) 2024 Yves Tanas
 * All Rights Reserved.
 *
 * This file is part of the Collections project.
 *
 * Unauthorized copying of this file, via any medium, is strictly prohibited.
 * Proprietary and confidential.
 *
 * This software may be used only as expressly authorized by the copyright holder.
 * Unless required by applicable law or agreed to in writing, software distributed
 * under this license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 *
 * For licensing inquiries, please contact: yves.tanas@example.com
 *
 * Contributors:
 *   Yves Tanas <yves.tanas@example.com>
 *
 * -------------------------------------------------------------------------------
 * File:        [GlobalLog.h]
 * Created:     [2025-06-12]
 * Description: [Short file/module purpose]
 * -------------------------------------------------------------------------------
 */

#pragma once

#define GET_CLASSNAME_WITH_FUNCTION			FString (__FUNCTION__)
#define GET_LINE_NUMBER						("Line:")+FString::FromInt(__LINE__)
#define GET_CURRENT_TIME					FDateTime::UtcNow().ToString()

//#define LOG_INFO	(TEXT("%s: %s [%s]"),*GET_CURRENT_TIME, *GET_CLASSNAME_WITH_FUNCTION, *GET_LINE_NUMBER)
