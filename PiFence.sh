#!/bin/bash

# Written by Brian Jenkins 
# 4 August 2022

# Send a UDP packet to the homemade power supply to cycle power to any one
# of the individual power supplies. This will serve to fence a Raspberry Pi
# that is powered by this supply. This script is used in conjuction with
# a pacemaker cluster setup with Raspberry Pis. 

# This script is based on the fence_ssh script written by Steve Bissaker
# See credits and GPL below
# https://github.com/nannafudge/fence_ssh/blob/alternate_version/fence_ssh

# Copyright (C) 2017  Steve Bissaker
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# Initialization
if [[ -z $OCF_ROOT ]]
then
  # Set default OCF_ROOT directory
  export OCF_ROOT=/usr/lib/ocf
fi

: ${OCF_FUNCTIONS_DIR=${OCF_ROOT}/resource.d/heartbeat}
. ${OCF_FUNCTIONS_DIR}/.ocf-shellfuncs

if [[ -z $__SCRIPT_NAME ]]
then
  __SCRIPT_NAME=$(basename $0)
fi

# Default action
action=$__OCF_ACTION
# Current node name
current_node_name=$(crm_node -n)
# Default user to ssh as
user='root'
description="${__SCRIPT_NAME} is a fencing agent for homemade USB power supply."

function usage()
{
cat <<EOF
$__SCRIPT_NAME - $description
Usage: $__SCRIPT_NAME -a|--action [action] -n|--nodename [nodename] [options]
Options:
 --help            This text
Commands:
 -a, --action      Action to perform: on|off/shutdown|reboot|monitor|metadata, defaults to reboot
 -n, --nodename    Node to fence
Options:
 -i, --fence-ip    IP address of the fence device, defaults to broadcast
 -P, --fence-port  Port of the fence device, default to 11089

EOF
    exit $OCF_SUCCESS;
}

function metadata()
{
cat <<EOF
<?xml version="1.0" ?>
<resource-agent name="${__SCRIPT_NAME}" shortdesc="Fencing agent for homemade USB power supply" >
  <longdesc>
    $description
  </longdesc>
  <parameters>
    <parameter name="action" unique="0" required="1">
    <getopt mixed="-a, --action"/>
    <content type="string" default="reboot"/>
    <shortdesc lang="en">Action to perform</shortdesc>
    <longdesc lang="en">The action to perform, can be one of: on|off/shutdown|reboot|monitor|metadata</longdesc>
    </parameter>
    <parameter name="nodename" unique="0" required="1">
      <getopt mixed="-n, --nodename"/>
      <content type="string"/>
      <shortdesc lang="en">Target nodename</shortdesc>
      <longdesc lang="en">The nodename of the remote machine to fence</longdesc>
    </parameter>
        <parameter name="ip" unique="0" required="0">
      <getopt mixed="-i, --fence-ip"/>
      <content type="string" default="255.255.255.255"/>
      <shortdesc lang="en">Fence IP address</shortdesc>
      <longdesc lang="en">The IP address of the fence device</longdesc>
    </parameter>
        <parameter name="fence-port" unique="0" required="0">
      <getopt mixed="-P, --fence-port"/>
      <content type="string" default="11089"/>
      <shortdesc lang="en">Fence port</shortdesc>
      <longdesc lang="en">The port of the fence device</longdesc>
    </parameter>
  </parameters>
  <actions>
    <action name="on" timeout="10s"/>
    <action name="off" timeout="10s"/>
    <action name="reboot" timeout="10s"/>
    <action name="monitor" timeout="10s" interval="120s"/>
    <action name="metadata" timeout="10s"/>
  </actions>
</resource-agent>
EOF
    exit 0;
}

# TODO: Needs to be completely changed
# Function to monitor the status of each power supply and report back to pacemaker
function monitor() {
  # Only need errors from cat here really
  ps_output=$(ps -p $(cat /run/sshd.pid) > /dev/null 2>&1)

  if [[ $? != 0 ]]
  then
    ocf_Log err "${__SCRIPT_NAME}: SSHD NOT running on resource ${current_node_name}: ${ps_output}"
    return $OCF_NOT_RUNNING
  fi

  ocf_log debug "${__SCRIPT_NAME}: SSHD is running, ${current_node_name} is availble for fencing if need be."
  return $OCF_SUCCESS
}

# TODO: Nneed to change to the actual action
# Function to send a UDP packet to the power supply and perform the fencing
# $1 is the command to perform (on | shutdown | reboot)
function perform_action() {
  command="nc -q0 -p 11089"
  real_action=$1

  if [[ -z $host ]]
  then
    ocf_log err "Error, no nodename specified! Run '${__SCRIPT_NAME} --help' for usage."
    return $OCF_ERR_ARGS
  fi

  if [[ $sudo == 'true' ]]
  then
    real_action="sudo ${real_action}"
  fi

  # add the destination ip to the end of the command
  if [[ -n $fence_ip ]]
  then
    command="${command} ${fence_ip}"
  else
    command="${command} 255.255.255.255"
  fi
  # add the destination ip to the end of the command
  if [[ -n $fence_port ]]
  then
    command="${command} ${fence_port}"
  else
    command="${command} 11089"
  fi

  ocf_log debug "${__SCRIPT_NAME}: About to execute STONITH command '${command} ${user}@${host} '${real_action}''"

  err_output=$(eval "echo '${real_action} ${host}' | ${command}" 2>&1)
  exit_code=$?

  if [[ $err_output != *'closed by remote host'* && $exit_code != 0 ]]
  then
    ocf_log err "Error fencing machine with ssh: ${err_output}"
    return $OCF_ERR_GENERIC
  fi

  ocf_log debug "Machine ${host} successfully fenced"
  return $OCF_SUCCESS
}

# Data being piped in from stonith-ng
if [[ -p /dev/stdin ]]
then
  while read line
  do
    raw_input+="${line} "
  done
  # Suffix args with double dash so it can be parsed by the case statement below
  args=$(echo $raw_input | awk '{ $0="--"$0; gsub(/ /, " --", $0); gsub(/=/, " ", $0); print $0 }')
  ocf_log debug "${__SCRIPT_NAME}: ARGS ARE $args"
  eval set -- "$args"
fi

while [[ $# -gt 0 ]]
do
  case $1 in
    -a | --action)
      action=$2
      shift
      shift
    ;;
    -n | --nodename)
      host=$2
      shift
      shift
    ;;
    -i | --fence-ip)
      fence_ip=$2
      shift
      shift
    ;;
    -P | --fence-port)
      fence_port=$2
      shift
      shift
    ;;
    --help)
      usage
    ;;
    --port)
      shift
      shift
    ;;
    *)
      unknown_args+=($1)
      shift
      shift
    ;;
  esac
done

if [[ -z $action ]]
then
  ocf_log err "Error, no action specified! Run '${__SCRIPT_NAME} --help' for usage."
  exit $OCF_ERR_ARGS
fi

for unknown_arg in ${unknown_args[@]};
do
  ocf_log err "Argument '${unknown_arg}' is not a valid argument!"
done

case $action in
  on)
    perform_action 'on'
  ;;
  off | shutdown)
    perform_action 'shutdown'
  ;;
  reboot)
    perform_action 'reboot'
  ;;
  monitor)
    monitor
  ;;
  metadata | meta_data | metadata)
    metadata
  ;;
  *)
    ocf_log info "Action ${action} is not a valid action, run '${__SCRIPT_NAME} --help' for usage."
    exit $OCF_ERR_UNIMPLEMENTED
  ;;
esac

exit $?