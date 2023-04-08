#!/bin/sh
COS_HOME=..
$COS_HOME/cal -l - -t $COS_HOME/cos-interface/systxt hello log adv
$COS_HOME/ldr -m - hello log adv
