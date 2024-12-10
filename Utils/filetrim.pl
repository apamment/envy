#!/usr/bin/env perl

use DBI;
use strict;

if ($#ARGV < 0) {
    print("Usage: ./filetrim.pl path/to/db.sqlite3\n");
    exit(-1);
}

my $database = $ARGV[0];

my $driver   = "SQLite"; 
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

$stmt = "SELECT id, filename FROM files";

my $sth = $dbh->prepare( $stmt );
$rv = $sth->execute() or die $DBI::errstr;
if($rv < 0){
    print $DBI::errstr;
}

my $files = 0;
my $deleted = 0;

while(my @row = $sth->fetchrow_array()) {
    $files++;
    print($row[1] . "\n");
    if ( ! -e $row[1] ) {
        my $stmt2 = "DELETE FROM files WHERE id = ?";
        my $sth2 = $dbh->prepare($stmt2);
        $sth2->execute($row[0]);
        $deleted++;
    }
}

print("Deleted $deleted missing files from $files total file entries.\n");

$dbh->disconnect();