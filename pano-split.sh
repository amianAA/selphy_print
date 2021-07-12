#!/bin/bash
###########

# Splits a panorama image into overlapping panels of the correct size.
#
# Note that this is _very_ preliminary, doing the fading/overlapping in a
# very naive linear manner.
#
# Also note that this script will probably go away as its functionality
# is moved into selphy_print &| gutenprint
#
infile="$1"
outfile="$2"
model="$3"
printsize="$4"

if [ -z "$5" ] ; then
    dpi=300
else
    dpi="$5"
fi

# D90
case "${model}" in
    kodak-6900|kodak-6950)
	overlap=600 # XXX Not sure yet
	if [ "${printsize}" eq "6x20" ] ; then
	    cols=1844
	    inrows=6108 # XXX Not sure
	    drows=2436
	elif [ "${printsize}" eq "6x18" ] ; then
	    cols=1844
	    inrows=5508 # XXX Not sure
	    drows=2436
	elif [ "${printsize}" eq "6x14" ] ; then
	    cols=1844
	    inrows=4308 # XXX Not sure
	    drows=2436
	elif [ "${printsize}" eq "6x12" ] ; then
	    cols=1844
	    inrows=3708 # XXX Not sure
	    drows=2436
	elif [ "${printsize}" eq "5x20" ] ; then
	    cols=1548
	    inrows=6108 # XXX Not sure
            overlap=150 # XXX Not sure
	    drows=2136
	elif [ "${printsize}" eq "5x15" ] ; then
	    cols=1548
	    inrows=4608 # XXX Not sure
	    drows=2136
	elif [ "${printsize}" eq "5x10" ] ; then
	    cols=1548
	    inrows=3108 # XXX Not sure
	    drows=2136
	else
	    echo "EK69xx supports 6x20, 6x18, 6x14, 6x12, 5x20, 5x15, and 5x10 only";
	fi
    ;;
    kodak-8810)
	cols=2464
	overlap=360
	if [ "${printsize}" eq "8x14" ] ; then
	    inrows=4224
	    drows=3024 # or 3624
	elif [ "${printsize}" eq "8x16" ] ; then
	    inrows=4824
	    drows=3024 # or 3624
	elif [ "${printsize}" eq "8x20" ] ; then
	    inrows=6024
	    drows=3024 # or 3624?
	elif [ "${printsize}" eq "8x24" ] ; then
	    inrows=7224
	    drows=3024 # or 3624, with overlap=180
	elif [ "${printsize}" eq "8x32" ] ; then
	    inrows=9624
	    drows=3624
	elif [ "${printsize}" eq "8x36" ] ; then
	    overlap=180
	    inrows=10824
	    drows=3624
	else
	    echo "EK8810 supports 8x14, 8x16, 8x20, 8x32, and 8x36 only";
	fi
    ;;
    mitsubishi-d90dw)
	cols=1852
	drows=2428
	overlap=628  # 600 + 28
	if [ "${printsize}" eq "6x20" ] ; then
	    inrows=6028
	elif [ "${printsize}" eq "6x14" ] ; then
	    inrows=4228
	else
	    echo "D90 supports 6x20 and 6x14 only"
	    exit 1
	fi
    ;;
    dnp-ds620)
	cols=1844
	drows=2436
	overlap=600
	if [ "${printsize}" eq "6x20" ] ; then
	    inrows=6108
	elif [ "${printsize}" eq "6x14" ] ; then
	    inrows=4272
	else
	    echo "DS620 supportx 6x20 and 6x14 only"
	    exit 1
	fi
	;;
    dnp-ds820)
	overlap=600
	cols=2448
	if [ "${printsize}" eq "8x18" ] ; then
	    drows=3036
	    inrows=5472
	elif [ "${printsize}" eq "8x26" ] ; then
	    drows=3036
	    inrows=7908
	elif [ "${printsize}" eq "8x22" ] ; then
	    drows=3636
	    inrows=6672
	elif [ "${printsize}" eq "8x32" ] ; then
	    drows=3636
	    inrows=9708
	else
	    echo "DS820 supportx 8x18, 8x26, 8x22, and 8x32 only"
	fi
	;;
    *)
	echo "Unsupported model!"
	exit 1
	;;
esac

# For 600dpi, double everything!
if [ ${dpi} == 600 ] ; then
    overlap=`expr ${overlap} * 2`
    cols=`expr ${cols} * 2`
    drows=`expr ${drows} * 2`
    inrows=`expr ${inrows} * 2`
fi

# Calculate page count XXX is this sane?
pages=`expr ${drows} - ${overlap}`
pages=`expr ${inrows} / ${pages}`

# Create gradients
fadeinname=fade_in_${cols}.pgm
fadeoutname=fade_out_${cols}.pgm

gm convert -size ${cols}x${overlap} gradient:white-black ${fadeinname}
gm convert -size ${cols}x${overlap} gradient:black-white ${fadeoutname}

# Generate panels
src_offset=0
panels=''
for panel in `seq 1 ${pages}` ; do
    echo "generating $panel / $pages"

    panelname=panel_${panel}.ppm
    panels="${panels} ${panelname}"

    # Generate panel
    gm convert ${infile} -crop ${cols}x${drows}+0+${src_offset} ${panelname}

    # Increment offset
    src_offset=`expr ${src_offset} + ${drows}`;
    src_offset=`expr ${src_offset} - ${overlap}`;

    # First page fades out only
    if [ ${panel} == 1 ] ; then
	gm composite -compose screen -gravity south ${fadeoutname} ${panelname} ${panelname}
    fi

    # Middle pages fade in and out
    if [ ${panel} -gt 1 ] ; then
	  if [ ${panel} -lt ${pages} ] ; then
	      gm composite -compose screen -gravity south ${fadeoutname} ${panelname} ${panelname}
	  fi
    fi
    if [ ${panel} -gt 1 ] ; then
	  if [ ${panel} -lt ${pages} ] ; then
    	      gm composite -compose screen -gravity north ${fadeinname} ${panelname} ${panelname}
	  fi
    fi

    # Last page fades in only
    if [ ${panel} == ${pages} ] ; then
	gm composite -compose screen -gravity north ${fadeinname} ${panelname} ${panelname}
    fi

done

# Clean up templates
rm ${fadeinname} ${fadeoutname}

## Create PDF
gm convert -density ${dpi}x${dpi} ${panels} ${outfile}
