<!DOCTYPE busconfig PUBLIC
 "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>

  <!-- Only user root can own the QAptWorker service -->
  <policy user="root">
    <allow own="@QAPT_WORKER_RDN_VERSIONED@"/>
  </policy>

  <policy context="default">
    <allow send_destination="@QAPT_WORKER_RDN_VERSIONED@"/>
  </policy>

</busconfig>

