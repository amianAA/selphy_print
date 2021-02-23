#!/usr/bin/perl
#######################
#
#  Test harness code for the dyesub backend (standalone sample jobs)
#
#  Copyright (c) 2018-2020 Solomon Peachy <pizza@shaftnet.org>
#
#  The latest version of this program can be found at:
#
#    http://git.shaftnet.org/cgit/selphy_print.git
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License as published by the Free
#  Software Foundation; either version 3 of the License, or (at your option)
#  any later version.
#
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
#  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
#  for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
#          [http://www.gnu.org/licenses/gpl-3.0.html]
#
#  SPDX-License-Identifier: GPL-3.0+
#
#######################
use strict;
use IPC::Run qw( run );

my @copies_set = (1, 3);
my $retval = 0;
my $valgrind = 1;

my $rotor = 0;
my $rotor_circ = 0;
my $row = 0;

my $quiet = 1;
my $proc_count = 0;
my @children = ();
my $kid;
my $error = 0;

my $backend_exec = "./dyesub_backend";

$ENV{"TEST_MODE"} = "2";

if (!defined($ARGV[0])) {
    die ("need a csv file\n");
};

if (defined($ENV{"STP_PARALLEL"})) {
    $proc_count = $ENV{"STP_PARALLEL"};
};
if (defined($ENV{"STP_VERBOSE"})) {
    $quiet = !$ENV{"STP_VERBOSE"};
};

if ($proc_count > 1) {
    $quiet = 1;
    $rotor_circ = $proc_count;
    for (my $child_no = 0; $child_no < $proc_count; $child_no++) {
	$kid = fork();
	if ($kid == 0) {
	    last;
	} else {
	    push @children, $kid;
	    $rotor++;
	}
    }
}

if ($proc_count > 1 && $kid > 0) {
    # Parent in parallel case
    while ($proc_count > 0 && $kid > 0) {
	$kid = waitpid(-1, 0);
	if ($kid > 0 && $? > 0) {
	    $error++;
	}
    }
    $retval = $error;

} else {  # worker child!
    my $currow = 0;
    my $rval = 0;

    open (INFILE, "<$ARGV[0]") || die ("can't open csv\n");

    while (<INFILE>) {
	chomp;
	next if /^#/;

	if (defined($ARGV[1])) {
	    next if (index($_,$ARGV[1]) == -1);
	};

	# for great parallelism!
	next if ($rotor_circ && ($currow++ % $rotor_circ) != $rotor);

	s/(.+)#.*/$1/;
	my @row = split(/,/);

	$ENV{"BACKEND"} = $row[0];
	$ENV{"EXTRA_VID"} = $row[1];
	$ENV{"EXTRA_PID"} = $row[2];

	if (length($row[4])) {
	    $ENV{"MEDIA_CODE"} = $row[4];
	} else {
	    undef($ENV{"MEDIA_CODE"});
	}

	foreach my $i (@copies_set) {
	    my @args = ($backend_exec, "-d", $i, "testjobs/${row[3]}");
	    if ($valgrind) {
		if ($quiet) {
		    unshift(@args,"-q");
		}
		unshift(@args,"valgrind", "--track-origins=yes", "--leak-check=full");
	    }
	    if (!$quiet) {
		print join(":", @args) . "\n";
	    }

	    if ($quiet) {
		$rval = run \@args, ">", "/dev/null", "2>", "/dev/null";
	    } else {
		$rval = run \@args;
	    }
	    if (!$rval) {
		print("***** $row[0] $row[1] $row[2] $row[3] $row[4] $i ***** FAIL: $? \n");
		$error++;
	    }
	}

	if ($error == 0) {
	    print "***** $row[0] $row[1] $row[2] $row[3] $row[4] ***** PASS\n";
	}
    }
    $retval = $error;
    close (INFILE);
    exit ($retval);
}

exit($retval);
