#!/bin/bash
@@SUB_HEADER@@

#thisdir=$(cd $(dirname $0) && pwd)
#basedir=$(cd $thisdir/.. && pwd)
basedir=$(cd "@@BASEDIR@@"; pwd)
app=@@APP@@
nps="@@NPS@@"
uth_params=(
@@MADM_PARAMS@@
)
sweep_params=(
@@SWEEP_PARAMS@@
)
trials=@@TRIALS@@
machine=@@MACHINE@@
date=@@DATE@@
dry_run=@@DRYRUN@@

sub=@@SUB@@
token=@@TOKEN@@
job_queue=@@JOBQUEUE@@
job_np=@@JOBNP@@
job_time=@@JOBTIME@@

host=@@HOSTNAME@@


exampledir=$basedir/examples

uthrun=mpirun
uthrun=$basedir/tools/uthrun/uthrun

thisfile=$(cd $(dirname $0) && pwd)/$(basename $0)


make_paramstr() {
    local default=$1
    shift 1

    ruby -e "
        ARGV.select{|s| !s[/-/]}.
            join('.').
            tap{|s| break '$default' if s.empty? }.
            display
    " -- $*
}

get_next_number() {
    local datadir=$1
    local prefix=$2
    local suffix=$3

    ruby -e "
        Dir['$datadir/$prefix.*.$suffix'].
            map{|s| File.basename(s).gsub('$prefix.', '').to_i }.
            push(-1).
            max.succ.display
    "
}

strip() {
    ruby -e "'$*'.strip.display"
}

run() {
    local uth_param=$(strip $1)
    local app=$2
    local np=$3
    shift 3

    local params=$(strip $*)

    # make log file directory
    local appdir=$exampledir/$app
    local datadir=$appdir/data/$machine/$date

    # make log file path
    local paramstr=$(make_paramstr "none" "$params")
    local uth_paramstr=$(make_paramstr "default" "$uth_param")
    local prefix0=$app.$paramstr.$np.$uth_paramstr

    local prefix=$prefix0
#    local no=$(get_next_number $datadir "$prefix0" "out")
#    local prefix=$prefix0.$no

    # make tmp directory
    local tmpdir=$(mktemp -d $appdir/.tmp.XXXXXX)
    mkdir -p $datadir

    # go to a temporary direcotry
    pushd . > /dev/null
    cd $tmpdir

    # make commad string
    local options="$uth_param -x GASNET_BACKTRACE=1 -n $np"

    if [ "$sub" == "t2sub" ]; then
        options="$options -f $PBS_NODEFILE"
    fi

    # execute
    status=0
    if [ "$dry_run" == "" ]; then
#	echo "$uthrun $options $appdir/$app $params"
	$uthrun $options $appdir/$app $params | tee tmp.out
	status=${PIPESTATUS[0]}

	cat tmp.out >> $datadir/$prefix.out

	if [ "$sub" == "" ]; then
	    cat tmp.out >> $0.out
	fi

	rm -f tmp.out
    else
	cmd="uthrun $options $app $params"
	show_cmd=$(ruby -e "'$cmd'.gsub(/ -x GASNET_BACKTRACE=1/, '').display")
	echo "$show_cmd"
    fi

    # clean up
    if [ -f *.prof ]; then
	cat *.prof > $datadir/$prefix.prof
    fi
    if [ -f *.sslog ]; then
	cat *.sslog > $datadir/$prefix.sslog
    fi
    popd > /dev/null
    rm -rf $tmpdir

    # exit if failed executing the command
    if [ $status -ne 0 ]; then
        return 1
    fi

    return 0
}

sweep() {
    for np in $nps; do
#	echo "---- np = $np ----"
	for ((i = 0; i < ${#sweep_params[@]}; i++)); do
	    local p=${sweep_params[i]}
#	    echo "==== params = $p ===="
	    for ((j = 0; j < ${#uth_params[@]}; j++)); do
		for k in $(seq 0 $((trials - 1))); do
		    local mp=${uth_params[j]}
		    run "$mp" $app $np $p
		done
	    done
	done
    done
}

run-pjsub() {
    if [ "$PJM_O_WORKDIR" != "" ]; then
	sweep
    else
	local nodes=$(( $(($job_np + 15)) / 16 ))
#        local nodes=$job_np

	local params=
	local params="$params -N $app-$job_np"
	local params="$params -L 'rscgrp=$job_queue'"
	local params="$params -L 'node=$nodes' --mpi 'proc=$job_np'"
	local params="$params -o $0.out"
	local params="$params -e $0.err"
	local params="$params -m e"

	mkdir -p $(dirname $0)

	if [ "$job_time" != "" ]; then
	    local params="$params -L 'elapse=$job_time'"
	fi

	local cmd="pjsub $params $0"

	if [ "$dry_run" != "" ]; then
	    echo "$cmd"
	    sweep
	else
	    echo "$cmd"
	    eval "$cmd"
	fi
    fi
}

run-t2sub() {
    if [ "$PBS_O_WORKDIR" != "" ]; then
	sweep
    else
	local nodes=$(( $(($job_np + 11)) / 12 ))

	local params=
	local params="$params -N $app-$job_np"
	local params="$params -q $job_queue"
	local params="$params -l select=$nodes:ncpus=12:mpiprocs=12:mem=48gb"
	local params="$params -W group_list=$token"
	local params="$params -o $0.out"
	local params="$params -e $0.err"

        if [ "$job_time" != "" ]; then
            params="$params -l walltime=$job_time"
        fi

	local cmd="t2sub $params $0"

	if [ "$dry_run" != "" ]; then
	    echo "$cmd"
	    sweep
	else
	    echo "$cmd"
	    eval "$cmd"
	fi
    fi
}

run-qsub() {
    if [ "$PBS_O_WORKDIR" != "" ]; then
	sweep
    else
	local cores=$(cat /proc/cpuinfo | grep processor | wc -l)
	local nodes=$(( $(($job_np + $cores - 1)) / $cores ))

	local params=
	if [ $nodes -eq 1 ]; then
	    local params="$params -l nodes=$host:ppn=$cores"
	else
	    local params="$params -l nodes=$nodes:ppn=$cores"
	fi

	local params="$params -q $job_queue"
	local params="$params -o $0.out"
	local params="$params -e $0.err"

	local cmd="qsub $params $0"

	if [ "$dry_run" != "" ]; then
	    echo "$cmd"
	    sweep
	else
	    echo "$cmd"
	    eval "$cmd"
	fi
    fi
}

start() {
    case $sub in
	"") sweep ;;
	"pjsub") run-pjsub ;;
	"t2sub") run-t2sub ;;
	"qsub") run-qsub ;;
	*) echo '$sub value is invalid' ;;
    esac
}

start
