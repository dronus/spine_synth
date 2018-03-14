#include "usb_names.h"

struct usb_string_descriptor_struct usb_string_product_name = {
        2 + 11 * 2,
        3,
        {'S','p','i','n','e',' ','S','y','n','t','h'}
};

/*
// Also disables Teensy auto program mode, for production only!
struct usb_string_descriptor_struct usb_string_manufacturer_name = {
        2 + 2 * 2,
        3,
        {'N','F'}
};
*/
