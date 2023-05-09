#ifndef _ADAPTED_TINY_IR_RECEIVER_HPP
#define _ADAPTED_TINY_IR_RECEIVER_HPP

#include "TinyIR.h"
#include "digitalWriteFast.h"

#if !defined(IR_PIN)
  #define IR_PIN 2
#endif

TinyIRReceiverStruct TinyIRReceiverControl;

/**
 * Declaration of the callback function provided by the user application.
 * It is called every time a complete IR command or repeat was received.
 */
extern void handleReceivedIRData(uint16_t aAddress, uint8_t aCommand, bool isRepetition);

void setup_receiver_and_interrupts() {
  cli();
  pinModeFast(IR_PIN, INPUT);
  // any change on PD2
  EICRA |= 1 << ISC00;
  // activate interrupt on PD2
  EIMSK |= 1 << INT0;
  sei();
}

/**
 * The ISR (Interrupt Service Routine) of TinyIRRreceiver.
 * It handles the NEC protocol decoding and calls the user callback function on complete.
 * 5 us + 3 us for push + pop for a 16MHz ATmega
 */
ISR(INT0_vect)
{
  uint_fast8_t tIRLevel = digitalReadFast(IR_PIN);

  uint32_t tCurrentMicros = micros();
  uint16_t tMicrosOfMarkOrSpace = tCurrentMicros - TinyIRReceiverControl.LastChangeMicros;

  TinyIRReceiverControl.LastChangeMicros = tCurrentMicros;

  uint8_t tState = TinyIRReceiverControl.IRReceiverState;

  if (tIRLevel == LOW) {
      if (tMicrosOfMarkOrSpace > 2 * TINY_RECEIVER_HEADER_MARK) {
          tState = IR_RECEIVER_STATE_WAITING_FOR_START_MARK;
      }
      if (tState == IR_RECEIVER_STATE_WAITING_FOR_START_MARK) {
          tState = IR_RECEIVER_STATE_WAITING_FOR_START_SPACE;
          TinyIRReceiverControl.Flags = IRDATA_FLAGS_EMPTY;
      }

      else if (tState == IR_RECEIVER_STATE_WAITING_FOR_FIRST_DATA_MARK) {
          if (tMicrosOfMarkOrSpace >= lowerValue25Percent(TINY_RECEIVER_HEADER_SPACE)
                  && tMicrosOfMarkOrSpace <= upperValue25Percent(TINY_RECEIVER_HEADER_SPACE)) {
              TinyIRReceiverControl.IRRawDataBitCounter = 0;
              TinyIRReceiverControl.IRRawData.ULong = 0;
              TinyIRReceiverControl.IRRawDataMask = 1;
              tState = IR_RECEIVER_STATE_WAITING_FOR_DATA_SPACE;
          } else {
              tState = IR_RECEIVER_STATE_WAITING_FOR_START_MARK;
          }
      }

      else if (tState == IR_RECEIVER_STATE_WAITING_FOR_DATA_MARK) {
          if (tMicrosOfMarkOrSpace >= lowerValue50Percent(TINY_RECEIVER_ZERO_SPACE)
                  && tMicrosOfMarkOrSpace <= upperValue50Percent(TINY_RECEIVER_ONE_SPACE)) {
              tState = IR_RECEIVER_STATE_WAITING_FOR_DATA_SPACE;
              if (tMicrosOfMarkOrSpace >= 2 * TINY_RECEIVER_UNIT) {
                  TinyIRReceiverControl.IRRawData.ULong |= TinyIRReceiverControl.IRRawDataMask;
              }

              TinyIRReceiverControl.IRRawDataMask = TinyIRReceiverControl.IRRawDataMask << 1;
              TinyIRReceiverControl.IRRawDataBitCounter++;
          } else {
              tState = IR_RECEIVER_STATE_WAITING_FOR_START_MARK;
          }
      } else {
          tState = IR_RECEIVER_STATE_WAITING_FOR_START_MARK;
      }
  } else {
      if (tState == IR_RECEIVER_STATE_WAITING_FOR_START_SPACE) {
          if (tMicrosOfMarkOrSpace >= lowerValue25Percent(TINY_RECEIVER_HEADER_MARK)
                  && tMicrosOfMarkOrSpace <= upperValue25Percent(TINY_RECEIVER_HEADER_MARK)) {
              tState = IR_RECEIVER_STATE_WAITING_FOR_FIRST_DATA_MARK;
          } else {
              tState = IR_RECEIVER_STATE_WAITING_FOR_START_MARK;
          }
      }

      else if (tState == IR_RECEIVER_STATE_WAITING_FOR_DATA_SPACE) {
          if (tMicrosOfMarkOrSpace >= lowerValue50Percent(TINY_RECEIVER_BIT_MARK)
                  && tMicrosOfMarkOrSpace <= upperValue50Percent(TINY_RECEIVER_BIT_MARK)) {
              if (TinyIRReceiverControl.IRRawDataBitCounter >= TINY_RECEIVER_BITS) {
                  tState = IR_RECEIVER_STATE_WAITING_FOR_START_MARK;
                  handleReceivedTinyIRData(
                    TinyIRReceiverControl.IRRawData.UBytes[0],
                    TinyIRReceiverControl.IRRawData.UBytes[2],
                    TinyIRReceiverControl.Flags);
              } else {
                  tState = IR_RECEIVER_STATE_WAITING_FOR_DATA_MARK;
              }
          } else {
              tState = IR_RECEIVER_STATE_WAITING_FOR_START_MARK;
          }
      } else {
          tState = IR_RECEIVER_STATE_WAITING_FOR_START_MARK;
      }
  }
  TinyIRReceiverControl.IRReceiverState = tState;
}

#endif // _ADAPTED_TINY_IR_RECEIVER_HPP
