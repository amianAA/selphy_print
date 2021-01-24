#!/usr/bin/perl
#######################
#
#  Test harness code for the dyesub backend (gutenprint rendering)
#
#  Copyright (c) 2019 Solomon Peachy <pizza@shaftnet.org>
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

my $retval = 0;

my $id = 123;
my $user = "tester";
my $title = "image_test";
my $input_image = "testjobs/s3s-59.png";

my @pages_set = (1, 3);
my @copies_set = (1, 3);

my $valgrind = 0;
my $work_dir_base = "/tmp";

my $rotor = 0;
my $rotor_circ = 0;
my $row = 0;

my $pano_mode = 0;

my $quiet = 1;
my $proc_count = 0;
my @children = ();
my $kid;
my $error = 0;

my $genppd_exec = "/usr/sbin/cups-genppd.5.3";
my $convert_exec = "/usr/bin/convert";
my $pdftoraster_exec = "/usr/lib/cups/filter/pdftoraster";
my $rastertogutenprint_exec = "/usr/lib/cups/filter/rastertogutenprint.5.3";
my $backend_exec = "./dyesub_backend";
my $pano_exec = "./pano-split.sh";

$ENV{"STP_SUPPRESS_VERBOSE_MESSAGES"} = 1;
$ENV{"OMP_NUM_THREADS"} = 1;
$ENV{"TEST_MODE"} = "2";

if (!defined($ARGV[0])) {
    die ("need a csv file\n");
};
if (defined($ENV{"STP_VERBOSE"})) {
    $quiet = !$ENV{"STP_VERBOSE"};
};
if (defined($ENV{"STP_PARALLEL"})) {
    $proc_count = $ENV{"STP_PARALLEL"};
};
if (defined($ENV{"STP_GENPPD"})) {
    $genppd_exec = $ENV{"STP_GENPPD"};
}
if (defined($ENV{"STP_RASTERTOGUTENPRINT"})) {
    $rastertogutenprint_exec = $ENV{"STP_RASTERTOGUTENPRINT"};
}


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

} else {
    my $currow = 0;

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

	my $gp_name = $row[0];
	$ENV{"BACKEND"} = $row[0];
	$ENV{"EXTRA_VID"} = $row[1];
	$ENV{"EXTRA_PID"} = $row[2];
	my $work_dir = "${work_dir_base}/$currow";
	if (!mkdir($work_dir)) {
	    print("cannot crate work dir ${work_dir}\n");
	    $error++;
	    next;
	}

	if (length($row[3])) {
	    $ENV{"MEDIA_CODE"} = $row[3];
	} else {
	    undef($ENV{"MEDIA_CODE"});
	}

	my $options = "";

	my @gp_options = split(/;/,$row[4]);
	# generate options list
	foreach my $x (@gp_options) {
	    $options .= "$x ";
	}

	# See if we're in panorama mode
	if (defined($row[5])) {
	    $pano_mode = 1;
	    pop(@pages_set);  # 1 "page"
	    pop(@copies_set); # 1 copy
	}

	my $rval;
	my @args;

	# Generate PPD
	my $ppd_fname = "${work_dir}/stp-${gp_name}.5.3.ppd";

	$ENV{"PPD"} = $ppd_fname;
	$ENV{"DEVICE_URI"} = "gutenprint53+usb://$row[0]/12345678";

	if (!$quiet) {
	    print "PPD=$ENV{PPD}\n";
	    print "DEVICE_URI=$ENV{DEVICE_URI}\n";
	}

	@args = ($genppd_exec, "-p", $work_dir, "-Z", "-q", $gp_name);
	if (!$quiet) {
	    print join(":", @args) . "\n";
	}
	if ($quiet) {
	    $rval = run \@args, "1>", "/dev/null", "2>", "/dev/null";
	} else {
	    $rval = run \@args;
	}
	if (!$rval) {
	    print("***** $row[0] $row[1] $row[2] $row[3] '$row[4]' FAIL genppd $?: -- " . join(":", @args) . "\n");
	    $error++;
	    next;
	}

	foreach my $pages (@pages_set) {
	    if ($pano_mode) {
		print("... $row[0] $row[1] $row[2] $row[3]\n");
		# XXX generate panorama bitmap
		my $pano_tmp = $input_image; # XXX

		# Generate PDF by running through script.
		@args = ($pano_exec, $pano_tmp, "${work_dir}/$currow-${gp_name}.pdf", $gp_name, $row[5]);
		if (!$quiet) {
		    print join(":", @args) . "\n";
		}
		$rval = run \@args;
		if (!$rval) {
		    print("***** $row[0] $row[1] $row[2] $row[3] '$row[4]' '$row[5]' FAIL: pano-split.sh: $? -- " . join(":", @args) . "\n");
		    $error++;
		    next;
		}
	    } else {
		# generate PDF.
		@args = ($convert_exec);
		for (my $i = 0 ; $i < $pages ; $i++) {
		    push(@args, $input_image);
		}
		push(@args, "-density");
		push(@args, "300x300");
		push(@args, "${work_dir}/$currow-${gp_name}.pdf");
		if (!$quiet) {
		    print join(":", @args) . "\n";
		}
		$rval = run \@args;
		if (!$rval) {
		    print("***** $row[0] $row[1] $row[2] $row[3] '$row[4]' FAIL: convert: $? -- " . join(":", @args) . "\n");
		    $error++;
		    next;
		}
	    }

	    # Generate raster from PDF
	    @args = ($pdftoraster_exec, $id, $user, $title, 1, $options, "${work_dir}/$currow-${gp_name}.pdf");
	    if (!$quiet) {
		print join(":", @args) . "\n";
	    }
	    if ($quiet) {
		$rval = run \@args, ">", "${work_dir}/$currow-${gp_name}.raster", "2>", "/dev/null";
	    } else {
		$rval = run \@args, ">", "${work_dir}/$currow-${gp_name}.raster";
	    }
	    if (!$rval) {
		print("***** $row[0] $row[1] $row[2] $row[3] '$row[4]' FAIL: imagetoraster $?: $pages -- " . join(":", @args) . "\n");
		$error++;
		next;
	    }

	    foreach my $copies (@copies_set) {
		# Call raster2gutenprint
		@args = ($rastertogutenprint_exec, $id, $user, $title, $copies, $options);
		if ($valgrind) {
		    if ($quiet) {
			unshift(@args,"-q");
		    }
		    unshift(@args,"valgrind");
		}
		if (!$quiet) {
		    print join(":", @args) . "\n";
		}
		if ($quiet) {
		    $rval = run \@args, "<", "${work_dir}/$currow-${gp_name}.raster", ">", "${work_dir}/$currow-${gp_name}.raw", "2>", "/dev/null";
		} else {
		    $rval = run \@args, "<", "${work_dir}/$currow-${gp_name}.raster", ">", "${work_dir}/$currow-${gp_name}.raw";
		}
		if (!$rval) {
		    print("***** $row[0] $row[1] $row[2] $row[3] '$row[4]' FAIL: rastertogutenorint $?: $pages $copies -- " . join(":", @args) . "\n");
		    $error++;
		    next;
		}

		# Call backend using CUPS methodologies, using STDIN.
		@args = ($backend_exec, $id, $user, $title, $copies, $options);
		if ($valgrind) {
		    if ($quiet) {
			unshift(@args,"-q");
		    }
		    unshift(@args,"valgrind");
		}
		if (!$quiet) {
		    print join(":", @args) . "\n";
		}
		if ($quiet) {
		    $rval = run \@args, "<", "${work_dir}/$currow-${gp_name}.raw", "2>", "/dev/null";
		} else {
		    $rval = run \@args, "<", "${work_dir}/$currow-${gp_name}.raw";
		}
		if (!$rval) {
		    print("***** $row[0] $row[1] $row[2] $row[3] '$row[4]' FAIL: backend $?: $pages $copies -- " . join(":", @args) . "\n");
		    $error++;
		    next;
		}
	    }
	}
	print "***** $row[0] $row[1] $row[2] $row[3] '$row[4]' PASS\n";

	unlink ("${work_dir}/$currow-${gp_name}.pdf");
	unlink ("${work_dir}/$currow-${gp_name}.raster");
	unlink ("${work_dir}/$currow-${gp_name}.raw");
	unlink ($ppd_fname);
	rmdir ($work_dir);
    }

    $retval = $error;
    close(INFILE);
    exit($retval);
}

exit($retval);
