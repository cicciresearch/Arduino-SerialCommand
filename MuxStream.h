#pragma once
#include <Arduino.h>

class MuxStream : public Stream {
public:
  enum Source {
    NONE,
    USB,
    UART
  };

  MuxStream(Stream& usb, Stream& uart)
    : _usb(usb), _uart(uart), _lastSource(NONE) {}

  int available() override {
    return _usb.available() + _uart.available();
  }

  int read() override {
    if (_usb.available()) {
      _lastSource = USB;
      return _usb.read();
    }
    if (_uart.available()) {
      _lastSource = UART;
      return _uart.read();
    }
    return -1;
  }

  int peek() override {
    if (_usb.available()) return _usb.peek();
    if (_uart.available()) return _uart.peek();
    return -1;
  }

  void flush() override {
    if (_lastSource == USB)  _usb.flush();
    if (_lastSource == UART) _uart.flush();
  }

  size_t write(uint8_t c) override {
    switch (_lastSource) {
      case USB:  return _usb.write(c);
      case UART: return _uart.write(c);
      default:   return 0;
    }
  }

  Source lastSource() const {
    return _lastSource;
  }

private:
  Stream& _usb;
  Stream& _uart;
  Source  _lastSource;
};
