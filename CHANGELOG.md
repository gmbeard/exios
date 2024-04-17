## 0.2.0
### MINOR Changes:
- `Timer` IO object that waits for timer expiry events
- Thread safety: Ensures `ContextThread::run()` can be called by multiple threads
- `UnixSocket` and `UnixSocketAcceptor` for handling abstract UNIX domain socket communication
- `Signal` object for waiting on system signals
- `Event` IO object to allow firing and waiting on custom events
- Adds ability to cancel all I/O on an object

