<!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
<node>
  <interface name="@QAPT_WORKER_RDN_VERSIONED@">
    <signal name="transactionQueueChanged">
      <arg name="active" type="s" direction="out"/>
      <arg name="queued" type="as" direction="out"/>
    </signal>
    <method name="updateCache">
      <arg type="s" direction="out"/>
    </method>
    <method name="installFile">
      <arg type="s" direction="out"/>
      <arg name="file" type="s" direction="in"/>
    </method>
    <method name="commitChanges">
      <arg type="s" direction="out"/>
      <arg name="instructionsList" type="a{sv}" direction="in"/>
      <annotation name="org.qtproject.QtDBus.QtTypeName.In0" value="QVariantMap"/>
    </method>
    <method name="upgradeSystem">
      <arg type="s" direction="out"/>
      <arg name="safeUpgrade" type="b" direction="in"/>
    </method>
    <method name="downloadArchives">
      <arg type="s" direction="out"/>
      <arg name="packageNames" type="as" direction="in"/>
      <arg name="dest" type="s" direction="in"/>
    </method>
    <method name="writeFileToDisk">
      <arg type="b" direction="out"/>
      <arg name="contents" type="s" direction="in"/>
      <arg name="path" type="s" direction="in"/>
    </method>
    <method name="copyArchiveToCache">
      <arg type="b" direction="out"/>
      <arg name="archivePath" type="s" direction="in"/>
    </method>
  </interface>
</node>
