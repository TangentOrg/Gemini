#!/usr/bin/perl
# Copyright (C) 2001 NuSphere Corporation, All Rights Reserved.
# 
# This program is open source software.  You may not copy or use this 
# file, in either source code or executable form, except in compliance 
# with the NuSphere Public License.  You can redistribute it and/or 
# modify it under the terms of the NuSphere Public License as published 
# by the NuSphere Corporation; either version 2 of the License, or 
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# NuSphere Public License for more details.
# 
# You should have received a copy of the NuSphere Public License
# along with this program; if not, write to NuSphere Corporation
# 14 Oak Park, Bedford, MA 01730.


# Gemini Message Compiler
if ($#ARGV != 1)
{
  die "Usage: perl msgcomp.pl <input file> <output file>\n";
}

$infile = shift;
$outfile = shift;

open(INPUT, $infile) or die "Can't open $infile";

$in_message = 0;

&clear_msginfo;

while (<INPUT>)
{
  chomp;

  $first_char = substr($_, 0, 1);

  if ($first_char eq "#")
  {
    ; # comment - ignore (comments must not be in message blocks)
  }
  elsif ($first_char eq "<")
  {
    $in_message = 1;
  }
  elsif ($first_char eq ">")
  {
    if ($in_message == 1)
    {
      if ($msgnum && $msgid && $msg)
      {
        push @msgs, [ $msgnum, $msgid, $msg, $fatal, $shownum, $syserr, $coredump ];
      }
      else
      {
        die "Incomplete message in messages file";
      }
      $in_message = 0;
      &clear_msginfo;
    }
    else
    {
      die "Improper format in messages file";
    }
  }

  if ($in_message == 1)
  {
    &parse_tag($_, $tag, $tag_value);

    if (uc($tag) eq "MSGNUM")
    {
      $msgnum = $tag_value;
    }
    elsif (uc($tag) eq "MSGID")
    {
      $msgid = $tag_value;
    }
    elsif (uc($tag) eq "PROID")
    {
      ; # ignore Progress message number, informational only
    }
    elsif (uc($tag) eq "FATAL")
    {
      $fatal = $tag_value;
    }
    elsif (uc($tag) eq "SYSERR")
    {
      $syserr = $tag_value;
    }
    elsif (uc($tag) eq "COREDUMP")
    {
      $coredump = $tag_value;
    }
    elsif (uc($tag) eq "SHOWNUM")
    {
      $shownum = $tag_value;
    }
    elsif (uc($tag) eq "MSG")
    {
      $msg = &strip_msg_comments($tag_value);
    }
  }
}

&create_message_file;
&create_header_files;

close(INPUT);
exit;

sub parse_tag
{
  $eq_pos = index($_[0], "=");

  if ($eq_pos != 0)
  {
    if (substr($_[0], 0, 1) eq "<")
    {
      $tag = substr($_[0], 1, $eq_pos - 1);
    }
    else
    {
      $tag = substr($_[0], 0, $eq_pos);
    }
    $tag_value = substr($_[0], $eq_pos + 1);
  }
}

# remove substitution comments from message text
#
# example: Error %s<error-number>
# becomes: Error %s

sub strip_msg_comments
{
  my $msg = $_[0];

  $msg =~ s/(<.*?>)//g;

  return $msg;
}

sub clear_msginfo
{
  $msgnum = 0;
  $msgid = "";
  $msg = "";
  $fatal = "";
  $shownum = "";
  $syserr = "";
  $coredump = "";
}

sub fmt_message
{
    my $msgnum  = $_[0];
    my $msg     = $_[1];
    my $syserr  = $_[2];
    my $shownum = $_[3];
    my $fatal   = $_[4];
    my $coredump = $_[5];

    return (lc($syserr) eq "yes" ? "SYSTEM ERROR: " : "") .
           (lc($fatal) eq "yes" ? (lc($coredump) eq "yes" ? "%G" : "%g") : "") .
           $msg .
           (lc($shownum) eq "yes" ? " ($msgnum)" : "") . "\n";
}

sub create_message_file
{
  # sort @msgs by message number
  my @sorted = sort {@$a[0] cmp @$b[0]} @msgs;

  # use binary mode so we don't get \r\n at end of messages on DOS-based
  # systems
  open(OUTPUT, ">$outfile") or die "Can't open $outfile";
  binmode OUTPUT;

  # count the total number of characters used by the messages
  my $tot_len = 0;
  for $row (@sorted)
  {
    $tot_len += length(&fmt_message(@$row[0], @$row[2], @$row[5], @$row[4], @$row[3], @$row[6]));
  }

  # the file looks like:
  #
  # 00000256 - number of messages
  # 00007fe8 - total number of characters used by the messages
  # 00001388 - message number
  # 00000000 - message offset
  # 00001389 - message number
  # 0000004f - message offset
  # ...

  my $header = pack("NN", scalar(@sorted), $tot_len);
  syswrite(OUTPUT, $header, 8);

  # output the message index (msg #, msg offset, msg #, msg offset, ...)
  my $offset = 0;
  for $row (@sorted)
  {
    $msg_entry = pack("NN", @$row[0], $offset);
    syswrite(OUTPUT, $msg_entry, 8);
    $offset += length(&fmt_message(@$row[0], @$row[2], @$row[5], @$row[4], @$row[3], @$row[6]));
  }

  # output the messages
  for $row (@sorted)
  {
    print OUTPUT &fmt_message(@$row[0], @$row[2], @$row[5], @$row[4], @$row[3], @$row[6]);
  }

  close(OUTPUT);
}

sub create_header_files
{
  # sort @msgs by msg id (e.g., bkMSG001)
  my @sorted = sort {lc(@$a[1]) cmp lc(@$b[1])} @msgs;

  my $curr_prefix = "";

  for $row (@sorted)
  {
    if (lc(substr(@$row[1], 0, 2)) ne $curr_prefix)
    {
      if ($curr_prefix)
      {
        close(HEADER);
      }
      $curr_prefix = lc(substr(@$row[1], 0, 2));
      my $fn = $curr_prefix . "msg.h";
      open(HEADER, ">$fn");
    }

    print HEADER "#define @$row[1] @$row[0] /* " .
                 substr(@$row[2], 0, 50) . " */\n";
  }

  close(HEADER);
}

