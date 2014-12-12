<!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
<node>
  <interface name="@QAPT_WORKER_RDN_VERSIONED@.transaction">
    <property name="transactionId" type="s" access="read"/>
    <property name="userId" type="i" access="read"/>
    <property name="role" type="i" access="read"/>
    <property name="status" type="i" access="read"/>
    <property name="error" type="i" access="read"/>
    <property name="locale" type="s" access="read"/>
    <property name="proxy" type="s" access="read"/>
    <property name="debconfPipe" type="s" access="read"/>
    <property name="packages" type="a{sv}" access="read">
      <annotation name="org.qtproject.QtDBus.QtTypeName" value="QVariantMap"/>
    </property>
    <property name="isCancellable" type="b" access="read"/>
    <property name="isCancelled" type="b" access="read"/>
    <property name="exitStatus" type="i" access="read"/>
    <property name="isPaused" type="b" access="read"/>
    <property name="statusDetails" type="s" access="read"/>
    <property name="progress" type="i" access="read"/>
    <property name="downloadProgress" type="(sistts)" access="read">
      <annotation name="org.qtproject.QtDBus.QtTypeName" value="QApt::DownloadProgress"/>
    </property>
    <property name="untrustedPackages" type="as" access="read"/>
    <property name="downloadSpeed" type="t" access="read"/>
    <property name="downloadETA" type="t" access="read"/>
    <property name="filePath" type="s" access="read"/>
    <property name="errorDetails" type="s" access="read"/>
    <property name="frontendCaps" type="i" access="read"/>
    <signal name="propertyChanged">
      <arg name="role" type="i" direction="out"/>
      <arg name="newValue" type="v" direction="out"/>
    </signal>
    <signal name="finished">
      <arg name="exitStatus" type="i" direction="out"/>
    </signal>
    <signal name="mediumRequired">
      <arg name="label" type="s" direction="out"/>
      <arg name="mountPoint" type="s" direction="out"/>
    </signal>
    <signal name="configFileConflict">
      <arg name="currentPath" type="s" direction="out"/>
      <arg name="newPath" type="s" direction="out"/>
    </signal>
    <signal name="promptUntrusted">
      <arg name="untrustedPackages" type="as" direction="out"/>
    </signal>
    <method name="setProperty">
      <arg name="property" type="i" direction="in"/>
      <arg name="value" type="v" direction="in"/>
    </method>
    <method name="run">
    </method>
    <method name="cancel">
    </method>
    <method name="provideMedium">
      <arg name="medium" type="s" direction="in"/>
    </method>
    <method name="replyUntrustedPrompt">
      <arg name="approved" type="b" direction="in"/>
    </method>
    <method name="resolveConfigFileConflict">
      <arg name="currentPath" type="s" direction="in"/>
      <arg name="replace" type="b" direction="in"/>
    </method>
    <method name="setFrontendCaps">
        <arg name="caps" type="i" direction="in"/>
    </method>
  </interface>
</node>
