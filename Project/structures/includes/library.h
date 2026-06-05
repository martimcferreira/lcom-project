/**
 * @file library.h
 * @brief Ficheiro principal que agrupa as sub-bibliotecas e drivers de hardware.
 *
 * Centraliza as inclusões dos drivers (I/O) facilitando o `include`
 * no resto dos componentes lógicos.
 * 
 * @defgroup Library Core Library Includes
 * @ingroup Devices
 * @brief Agregação dos drivers do sistema (Minix).
 * @{
 */

#ifndef _PROJECT_LIBRARY_H_
#define _PROJECT_LIBRARY_H_

#include "drivers/bitwise.h"
#include "drivers/i8042.h"
#include "drivers/i8254.h"
#include "drivers/kbc.h"
#include "drivers/mouse.h"
#include "drivers/rtc.h"
#include "drivers/timer.h"
#include "drivers/utils.h"
#include "video.h"

#endif /* _PROJECT_LIBRARY_H_ */
/** @} */
