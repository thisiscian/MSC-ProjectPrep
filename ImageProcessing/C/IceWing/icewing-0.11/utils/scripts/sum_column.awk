#! /usr/bin/awk -f

BEGIN {
    sum = 0;
    if (ARGC > 2) {
	col = ARGV[1];
	delete ARGV[1];
    } else {
	col = 1;
    }
}

{
    sum += $col;
}

END {
    printf ("Sum of column %d: %d\n", col, sum);
}
