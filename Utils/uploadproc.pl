#!/usr/bin/env perl

my $tmp_path = "UNDEFINED";

use strict;


if ($#ARGV < 1) {
    print "Usage ./uploadproc.pl node file\n";
    exit(0);
}

my $node = $ARGV[0];
my $file = $ARGV[1];

my $string;

if ($tmp_path == "UNDEFINED") {
    print("Sysop please configure this script with your tmp_path!\r\n");
    exit(0);
}

print "Checking for FILE_ID.DIZ...";

if (uc(substr($file, -3)) == "ZIP") {
    mkdir("/tmp/uploadproc/$node");
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

        open(FH, '>', "$tmp_path/%node/file.desc") or die $!;
        print FH $string;
        close FH;
    } else {
        print(" no description.\n");
    }
    rmdir("/tmp/uploadproc/$node");
} 
