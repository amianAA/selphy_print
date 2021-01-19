#!/bin/bash
###########

# Splits a panorama image into overlapping panels of the correct size.
#
# Note that this is _very_ preliminary, doing the fading/overlapping in a
# very naive linear manner.

infile="$1"

# D90
dpi=300
cols=1852
drows=2428
overlap=600
## 6x20 3-pane
inrows=6084
## 6x14 2-pane
#inrows=4256

#DS620
#dpi=300
#cols=1844
#drows=2436
#overlap=600
## 6x20 3-page
#inrows=6108
## 6x14 2-page
#inrows=4272

#DS820
#dpi=300
#overlap=600
#cols=2448
## 8x18 2-pane
#drows=3036
#inrows=5472
## 8x26 3-pane
#drows=3036
#inrows=7908
## 8x22 2-pane
#drows=3636
#inrows=6672
## 8x32 3-pane
#drows=3636
#inrows=9708

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
gm convert -density ${dpi}x${dpi} ${panels} out.pdf
