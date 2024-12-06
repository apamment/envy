#!/usr/bin/env perl

use strict;
use Config::IniFiles;
use File::Glob qw(:globally :nocase);
use Digest::CRC;
use File::Copy;
use File::FnMatch qw(:fnmatch);
use DBI;

sub remove_from_database {
    my $driver   = "SQLite"; 
    my $dsn = "DBI:$driver:dbname=" . $_[1];
    my $userid = "";
    my $password = "";
    my $dbh = DBI->connect($dsn, $userid, $password, { RaiseError => 1 }) 
                      or die $DBI::errstr;

    my $stmt = qq(CREATE TABLE IF NOT EXISTS files(id INTEGER PRIMARY KEY, filename TEXT, description TEXT, uploaddate INTEGER, uploadedby TEXT, downloadcount INTEGER));
    my $rv = $dbh->do($stmt);
    my $curtime = time();

    my $sth = $dbh->prepare('SELECT id, filename FROM files');
    $sth->execute();
    while(my @row = $sth->fetchrow_array()) {
        if (fnmatch($_[1], basename($row[1]), FNM_CASEFOLD)) {
            # it matches!
            my $id = $row[0];

            unlink($row[1]);

            $sth = $dbh->prepare('DELETE FROM files WHERE id = $1');
            $sth->execute($id);
            $dbh->disconnect();
            return;
        }
    }
    $dbh->disconnect();    
}

sub add_to_database {
    my $driver   = "SQLite"; 
    my $dsn = "DBI:$driver:dbname=" . $_[1];
    my $userid = "";
    my $password = "";
    my $dbh = DBI->connect($dsn, $userid, $password, { RaiseError => 1 }) 
                      or die $DBI::errstr;

    my $stmt = qq(CREATE TABLE IF NOT EXISTS files(id INTEGER PRIMARY KEY, filename TEXT, description TEXT, uploaddate INTEGER, uploadedby TEXT, downloadcount INTEGER));
    my $rv = $dbh->do($stmt);
    my $curtime = time();

    my $sth = $dbh->prepare('INSERT INTO files (filename, description, downloadcount, uploadedby, uploaddate) VALUES($1, $2, 0, "Tic Processor", $3)');
    $sth->execute($_[2], $_[3], $curtime);

    $dbh->disconnect();
}

my $config = Config::IniFiles->new( -file => $ARGV[0]);

my @files = glob($config->val('main', 'inbound') . "/*.tic");

foreach my $fp (@files) {
    my $area = "";
    my $file = "";
    my $lfile = "";
    my $desc = "";
    my $ldesc = "";
    my $replaces = "";
    my $crc = "";
    my $password = "";

    open (my $fptr, "<", $fp) or die $!;

    while (my $line = <$fptr>) {
        chomp($line);
        if (uc(substr($line, 0, 5)) eq "AREA ") {
            $area = substr($line, 5);
        } elsif (uc(substr($line, 0, 4)) eq "FILE") {
            $file = substr($line, 5);
        } elsif (uc(substr($line, 0, 6)) eq "LFILE") {
            $lfile = substr($line, 6);
        } elsif (uc(substr($line, 0, 4)) eq "DESC") {
            $desc = substr($line, 5);
        } elsif (uc(substr($line, 0, 5)) eq "LDESC") {
            if ($ldesc eq "") {
                $ldesc = substr($line, 6);
            } else {
                $ldesc = $ldesc . "\n" . substr($line, 6);
            }
        } elsif (uc(substr($line, 0, 8)) eq "REPLACES") {
            $replaces = substr($line, 9);
        } elsif (uc(substr($line, 0, 3)) eq "CRC") {
            $crc = substr($line, 4);
        } elsif (uc(substr($line, 0, 2)) eq "PW") {
            $password = substr($line, 3);
        }
    }
    close $fptr;
    if ($area ne "") {

        if (uc($config->val(uc($area), "password")) == uc($password)) {
            if ($crc ne "") {
                my $ctx = Digest::CRC->new( type => 'crc32' );
                my $filename;

                if ($lfile != "") {
                    $filename = $lfile;
                } else { 
                    $filename = $file;
                }

                if ( ! -e $filename) {
                    $filename = lc($filename);
                }

                open my $fh, '<:raw', $config->val('main', 'inbound') . "/" . $filename or die $!;
                $ctx->addfile(*$fh);
                close $fh;

                if (uc($crc) eq uc($ctx->hexdigest)) {
                    # CRC32 matches.

                    if ($replaces ne "") {
                        remove_from_database($replaces);
                    }

                    # Copy file
                    copy($config->val('main', 'inbound') . "/" . $filename, $config->val(uc($area), "path") . $filename);
                    # add file to database
                    my $description;

                    if ($ldesc ne "") {
                        $description = $ldesc;
                    } else {
                        $description = $desc;
                    }

                    add_to_database($config->val(uc($area), "database"), $config->val(uc($area), "path") . $filename, $description);

                    # delete tic file & source file
                    unlink($config->val('main', 'inbound') . "/" . $filename);
                    unlink($fp)
                } else {
                    print "CRC-32 MISMATCH\n";
                }

            }
        } else {
            print "Missing area config - or password mismatch!\n";
        } 
    } 
}