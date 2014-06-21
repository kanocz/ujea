ujea
============

  simple event-based daemon which reads AMQP queue, executes jobs and returns stdout/stderr to queue


Build
=====
```
  git clone https://github.com/kanocz/ujea
  git git submodule init
  git submodule update
  qmake
  make
```

Deb package
===========
```
  git clone https://github.com/kanocz/ujea
  git git submodule init
  git submodule update
  cd ujea
  dpkg-buildpackage
```

Posting a job
=============

  For testing i used amqp-tools package, so simple command to tun a job in server "ubuntu-deve;" with is listening AMQP on our local server:
```
  amqp-publish -r ubuntu-devel_cmd -b '{"type":"exec","jobId":"1","cmd":"/bin/echo","args":["hello","world"]}'
```
  to get process and result of executing we have to exec
```
  amqp-get -q ubuntu-devel_rpl
```
  several times and get something like

```
  {
      "jobId": "1",
      "status": "started",
      "timestamp": 1403375012922,
      "type": "status"
  }
  {
      "data": "aGVsbG8gd29ybGQK",
      "jobId": "1",
      "timestamp": 1403375012922,
      "type": "stdOut"
  }
  {
      "exitCode": 0,
      "jobId": "1",
      "status": "stoped",
      "timestamp": 1403375012922,
      "type": "status"
  }
```

  Additional option is to pass "stdin" argument in base64 encoding. In this case ujea will send content of this argumet
 to the stdin of executed process and then close this stream to indicate end of input. Empty argument'll just close. No
 argument just not to close :)
  
  So you can post message like this:
```
  amqp-publish -r ubuntu-devel_cmd -b '{"type":"exec","jobId":"2","cmd":"/bin/cat","stdin":"SGVsbG8sIHdvcmxkIQ=="}'
```
  And get such result:
```
  {
      "jobId": "2",
      "status": "started",
      "timestamp": 1403376015562,
      "type": "status"
  }
  {
      "data": "SGVsbG8sIHdvcmxkIQ==",
      "jobId": "2",
      "timestamp": 1403376015564,
      "type": "stdOut"
  }
  {
      "exitCode": 0,
      "jobId": "2",
      "status": "stoped",
      "timestamp": 1403376015565,
      "type": "status"
  }
```
  on long-runnig jobs you may get more "stdOut" and/or "stdErr" messages during execution
  
  **IMPORTANT: jobId must be unique for every job (cat be any string), exec message with duplicated jobId will be ignored**
  
  it's possible to terminate running job in 2 ways (using TERM and KILL signals):
```
    amqp-publish -r ubuntu-devel_cmd -b '{"type":"terminate","jobId":"3"}'
```
```
    amqp-publish -r ubuntu-devel_cmd -b '{"type":"kill","jobId":"4"}'
```
  in this case replays'll look like
```
  {
      "jobId": "4",
      "message": "Crashed",
      "status": "error",
      "timestamp": 1403376399123,
      "type": "status"
  }
  {
      "exitCode": 0,
      "jobId": "4",
      "status": "stoped",
      "timestamp": 1403376399123,
      "type": "status"
  }
```
  that's all :) if you have any questions, please email me