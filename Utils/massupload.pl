#!/usr/bin/env perl

use DBI;
use strict;


if ($#ARGV < 1) {
    print "Usage ./massupload.pl filedir database.sqlite3\n";
    exit(0);
}


my $dir = $ARGV[0];

if (substr($dir, 1) != '/') {
    print "Please use the full path of the directory you wish to scan.\n";
    exit(0);
}

my $driver   = "SQLite"; 
my $database = $ARGV[1];
my $dsn = "DBI:$driver:dbname=$database";
my $userid = "";
my $password = "";
my $dbh = DBI->connect($dsn, $userid, $password, { RaiseError => 1 }) 
                      or die $DBI::errstr;

my $stmt = qq(CREATE TABLE IF NOT EXISTS files(id INTEGER PRIMARY KEY, filename TEXT, description TEXT, uploaddate INTEGER, uploadedby TEXT, downloadcount INTEGER));
my $rv = $dbh->do($stmt);

my $string;

if($rv < 0){
   print $DBI::errstr;
   exit(0);
} 

print "Scanning " . $dir . " ...\n";

foreach my $fp (glob("$dir/*")) {
  
  if ( -f $fp) {
      print ".. Found: " . $fp . " ... ";

      $stmt = 'SELECT count(*) FROM files WHERE filename=$1;';
      my $sth = $dbh->prepare( $stmt );
      $rv = $sth->execute($fp) or die $DBI::errstr;
      if($rv < 0){
         print $DBI::errstr;
      }
      if ($sth->fetch()->[0]) {
         print " duplicate.\n";
      } else {

        if (uc(substr($fp, -3)) == "ZIP") {
            mkdir("/tmp/massupload");
            system("unzip -jCLL $fp file_id.diz -d /tmp/massupload > /dev/null 2>&1");
            if ( -f "/tmp/massupload/file_id.diz") {
                print(" found description.\n");
                local $/=undef;
                open FILE, "/tmp/massupload/file_id.diz" or die "Couldn't open file: $!";
                binmode FILE;
                $string = <FILE>;
                close FILE;

                $string =~ s/\r//g;
                unlink("/tmp/massupload/file_id.diz");
            } else {
                $string = "No Description.";
                print(" no description.\n");
            }
            rmdir("/tmp/massupload");
        } else {
            $string = "No Description.";
            print(" not zip file.\n");
        }
        my $curtime = time();
        $sth = $dbh->prepare('INSERT INTO files (filename, description, downloadcount, uploadedby, uploaddate) VALUES($1, $2, 0, "Mass Upload", $3)');
        $sth->execute($fp, $string, $curtime);
    }
  }
}
