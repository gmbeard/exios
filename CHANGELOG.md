## 0.4.0
### MINOR Changes:
- Changes project to `LGPL-3.0-or-later` license

## 0.3.0
### MINOR Changes:
- Adds *semaphore* operation mode to `Event`

### PATCH Changes:
- Fixes a bug that sometimes caused a read outside a valid iterator range when cancelling I/O
- Fix for timer to allow zero duration timeouts. This bypasses the I/O wait and posts the completion immediately.
- Fix: Ensure the event interest flags (`EPOLLIN`, etc.) are reset after processing I/O for notified FDs
- Fixes a potential deadlock when cancelling I/O in completion handlers
- Fix to I/O rescheduling to avoid an extra loop when determining the updated interest flags
- Adds missing `cancel()` operation on Event
- Fixes a bug where the signal details weren't being returned in the signal wait completion result
- Fixes a deadlock that can happend when io-based and non-io-based operations are queued

## 0.2.0
### MINOR Changes:
- `Timer` IO object that waits for timer expiry events
- Thread safety: Ensures `ContextThread::run()` can be called by multiple threads
- `UnixSocket` and `UnixSocketAcceptor` for handling abstract UNIX domain socket communication
- `Signal` object for waiting on system signals
- `Event` IO object to allow firing and waiting on custom events
- Adds ability to cancel all I/O on an object

