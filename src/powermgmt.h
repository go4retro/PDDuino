/*
 * sleep.h
 *
 *  Created on: Sep 20, 2020
 *      Author: brain
 */

#ifndef POWERMGMT_H_
#define POWERMGMT_H_

#if defined(ENABLE_SLEEP)
void wakeNow (void);
void sleepNow(void);
#else
#define wakeNow()   do {} while(0)
#define sleepNow()  do {} while(0)
#endif

#endif /* POWERMGMT_H_ */
