
static Result parTreeSearch_fj(counter_t depth, Node parent, size_t task_depth);

static Result
doParTreeSearch_fj(counter_t depth, Node parent, int childType,
                   counter_t numChildren, counter_t begin, counter_t end,
                   size_t task_depth)
{
    if (end - begin == 1) {
        Node child;
        makeChild(&parent, childType, computeGranularity, (int)begin,
                  &child);
        return parTreeSearch_fj(depth, child, task_depth);
    } else {
        counter_t center = (begin + end) / 2;
        counter_t n_left = center - begin;
        counter_t n_right = end - center;

        madm::future<Result> f(doParTreeSearch_fj,
                               depth, parent, childType, numChildren,
                               begin, center, task_depth + 1);

        Result r1 = doParTreeSearch_fj(depth, parent, childType, numChildren,
                                       center, end, task_depth);

        Result r0 = f.touch();

        return mergeResult(r0, r1);
    }
}

static Result parTreeSearch_fj(counter_t depth, Node parent,
                               size_t task_depth) {
    counter_t i;
    int j;
    Result result;

    assert(depth == 0 || parent.height > 0);

    counter_t numChildren = uts_numChildren(&parent);
    int childType = uts_childType(&parent);

    // Recurse on the children
    if (numChildren == 0) {
        Result r = { depth, 1, 1 };
        result = r;
    } else {

        result = doParTreeSearch_fj(depth + 1, parent, childType, numChildren,
                                    0, numChildren, task_depth);
        
        result.size += 1;
    }

    return result;
}

static Result
uts_fj_main(double *walltime)
{
    Result r;
    Node root;
    madm::pid_t me = madm::get_pid();

//    madm_prof_start();

    if (me == 0) {
        uts_initRoot(&root, type);

        double t1 = uts_wctime();

        madm::future<Result> f(parTreeSearch_fj, (counter_t)0, root, (size_t)0);
        r = f.touch();

        double t2 = uts_wctime();

        *walltime = t2 - t1;
    }
    madm::barrier();

//    madm_prof_stop();

    return r;
}
