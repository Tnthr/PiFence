#!/bin/bash

# Written by Brian Jenkins 
# 4 August 2022

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
description="${__SCRIPT_NAME} is a basic fencing agent that uses SSH."

function usage()
{
cat <<EOF
$__SCRIPT_NAME - $description
Usage: $__SCRIPT_NAME [action] -n|--nodename [nodename] [Additional Options...]
Options:
 --help 	   This text
Commands:
 -a, --action      Action to perform: off/shutdown|reboot|monitor|metadata, defaults to reboot
 -n, --nodename    Node to fence
Additional Options:
 -u, --user        OPTIONAL: User to ssh as, defaults to root
 -s, --sudo        OPTIONAL: true/false: Whether to execute the action using sudo
 -p, --password    OPTIONAL: Password of host to fence
 -k, --private-key OPTIONAL: The private key to use for SSH authentication
EOF
    exit $OCF_SUCCESS;
}

function metadata()
{
cat <<EOF
<?xml version="1.0" ?>
<resource-agent name="${__SCRIPT_NAME}" shortdesc="Basic fencing agent that uses SSH" >
  <longdesc>
    $description
  </longdesc>
  <parameters>
    <parameter name="action" unique="0" required="1">
    <getopt mixed="-o, --action"/>
    <content type="string" default="reboot"/>
    <shortdesc lang="en">Action to perform</shortdesc>
    <longdesc lang="en">The action to perform, can be one of: off/shutdown|reboot|monitor|metadata</longdesc>
    </parameter>
    <parameter name="user" unique="0" required="1">
      <getopt mixed="-u, --user"/>
      <content type="string" default="root"/>
      <shortdesc lang="en">User to SSH as on remote</shortdesc>
      <longdesc lang="en">The user to SSH as and execute the STONITH command as on the remote machine</longdesc>
    </parameter>
    <parameter name="nodename" unique="0" required="0">
      <getopt mixed="-n, --nodename"/>
      <content type="string"/>
      <shortdesc lang="en">Target nodename</shortdesc>
      <longdesc lang="en">The nodename of the remote machine to fence</longdesc>
    </parameter>
    <parameter name="password" unique="0" required="0">
      <getopt mixed="-p, --password"/>
      <content type="string"/>
      <shortdesc lang="en">Target password</shortdesc>
      <longdesc lang="en">The password of the remote SSH user (optional)</longdesc>
    </parameter>
    <parameter name="private-key" unique="0" required="0">
      <getopt mixed="-k, --private-key"/>
      <content type="string"/>
      <shortdesc lang="en">Target private key</shortdesc>
      <longdesc lang="en">SSH private key to authenticate with (optional)</longdesc>
    </parameter>
    <parameter name="sudo" unique="0" required="0">
      <getopt mixed="-s, --sudo"/>
      <content type="boolean"/>
      <shortdesc lang="en">Whether to execute using sudo</shortdesc>
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

# $1 is the SSH command to perform
function perform_ssh() {
  ssh_command="ssh"
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

  if [[ -n $password ]]
  then
    ssh_command="sshpass -p${password} ssh"
  fi

  if [[ -n $private_key ]]
  then
    ssh_command="ssh -i ${private_key}"
  fi

  ocf_log debug "${__SCRIPT_NAME}: About to execute STONITH command '${ssh_command} ${user}@${host} '${real_action}''"

  err_output=$(eval "${ssh_command} ${user}@${host} -o StrictHostKeyChecking=no '${real_action}'" 2>&1)
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
    -o | --action)
      action=$2
      shift
      shift
    ;;
    -u | --user)
      user=$2
      shift
      shift
    ;;
    -n | --nodename)
      host=$2
      shift
      shift
    ;;
    -p | --password)
      password=$2
      shift
      shift
    ;;
    -k | --private-key)
      private_key=$2
      shift
      shift
    ;;
    -s | --sudo)
      sudo=$2
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
    ocf_log debug "'On' action has not been implemented for this agent."
    exit $OCF_SUCCESS
    #exit $OCF_ERR_UNIMPLEMENTED
  ;;
  off | shutdown)
    perform_ssh 'shutdown -h now'
  ;;
  reboot)
    perform_ssh 'shutdown -r now'
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