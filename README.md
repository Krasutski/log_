# The "LOG_"
Just Logger but development for embedded devices 

```
#include "log_io.h"

int main (void) {
   #if LOG_ENABLED == 1U
      log_init(LOG_MASK_ALL, &log_io_interface);
   #endif  // LOG_ENABLED == 1U
   
    LOG_INFO("\r\nApplication(" __DATE__ " " __TIME__") Started\r\n");
   
   for(;;) {
   }
}
```