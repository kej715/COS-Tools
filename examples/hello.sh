#!/bin/sh
COS_HOME=..
$COS_HOME/cal -l - -t $COS_HOME/cos-interface/systxt hello log adv
$COS_HOME/lib -l - -o sys.lib log.obj adv.obj
$COS_HOME/ldr -m - hello sys.lib
