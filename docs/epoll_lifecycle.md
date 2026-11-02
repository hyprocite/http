# EPOLL_LIFECYCLE


## EPOLL_CTL_ADD
 -一个用于epoll_ctl 的操作参数，同于添加监听回调fd

### event_loop.cpp
```c
    //line 39 
```


 ## EPOLL_CTL_MOD
  - 同操作参数， 用于修改fd的监听状态参数

  ## EPOLL_CTL_DEL

  - 删除epoll管理的fd，停止监听后续发生事件(删除后，原有待回调通过shareptr持有着)