#!/usr/bin/env perl

use strict;
use File::Path;

# Change this to the full path of yout tmp path in envy.ini
my $tmp_path = "UNDEFINED";

if ($#ARGV < 1) {
    print "Usage ./uploadproc.pl node file\n";
    exit(0);
}

my $node = $ARGV[0];
my $file = $ARGV[1];

my $string;

if ($tmp_path eq "UNDEFINED") {
    print("Sysop please configure this uploadproc.pl with your tmp_path!\r\n");
    exit(0);
}

print "Checking for FILE_ID.DIZ...";

if (uc(substr($file, -3)) == "ZIP") {
    File::Path::make_path("/tmp/uploadproc/$node");
    system("unzip -jCLL $file file_id.diz -d /tmp/uploadproc/$node > /dev/null 2>&1");
    if ( -f "/tmp/uploadproc/$node/file_id.diz") {
        print(" found description.\r\n");
        local $/=undef;
        open FILE, "/tmp/uploadproc/$node/file_id.diz" or die "Couldn't open file: $!";
        binmode FILE;
        $string = <FILE>;
        close FILE;

        $string =~ s/\r//g;
        unlink("/tmp/uploadproc/$node/file_id.diz");

        open(FH, '>', "$tmp_path/$node/file.desc") or die $!;
        print FH $string;
        close FH;
    } else {
        print(" no description.\r\n");
    }
    File::Path::remove_tree("/tmp/uploadproc/$node");
} else {
    print(" no description.\r\n");
}
