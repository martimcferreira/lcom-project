# lab3

Self-contained Lab 3 project for LCOM.

## Files

- `lab3.c` - main entry point and implementations of:
  - `kbd_test_scan()`
  - `kbd_test_poll()`
  - `kbd_test_timed_scan(uint8_t n)`
- `kbc.c`, `kbc.h` - keyboard interrupt subscription, interrupt handler, polling reads, and KBC command-byte helpers
- `timer_local.c`, `timer_local.h` - minimal timer interrupt support used by `kbd_test_timed_scan()`
- `utils.c`, `utils.h` - `util_sys_inb()` wrapper with syscall counting
- `i8042.h` - keyboard-controller constants
- `Makefile` - builds `lab3` with `make`

## Run

```sh
lcom_run lab3 "scan"
lcom_run lab3 "poll"
lcom_run lab3 "timed 5"
```

## Notes

- This zip is built to be self-contained, so it does not depend on a sibling `lab2` folder.
- The official guide mentions a provided `lab3.c` stub. Since that file is not conveniently exposed through the guide viewer, this project recreates the expected LCF layout and required functions from the lab specification.
