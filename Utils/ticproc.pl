#!/usr/bin/env perl

use strict;
use Config::IniFiles;
use File::Glob qw(:globally :nocase);
use Digest::CRC;
use File::Copy;
use File::Basename;
use File::FnMatch qw(:fnmatch);
use DBI;

sub remove_from_database {
    my $driver   = "SQLite"; 
    my $dsn = "DBI:$driver:dbname=" . $_[0];
    my $userid = "";
    my $password = "";
    my $dbh = DBI->connect($dsn, $userid, $password, { RaiseError => 1 }) 
                      or die $DBI::errstr;

    my $stmt = qq(CREATE TABLE IF NOT EXISTS files(id INTEGER PRIMARY KEY, filename TEXT, description TEXT, uploaddate INTEGER, uploadedby TEXT, downloadcount INTEGER));
    my $rv = $dbh->do($stmt);

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
    my $dsn = "DBI:$driver:dbname=" . $_[0];
    my $userid = "";
    my $password = "";
    my $dbh = DBI->connect($dsn, $userid, $password, { RaiseError => 1 }) 
                      or die $DBI::errstr;

    my $stmt = qq(CREATE TABLE IF NOT EXISTS files(id INTEGER PRIMARY KEY, filename TEXT, description TEXT, uploaddate INTEGER, uploadedby TEXT, downloadcount INTEGER));
    my $rv = $dbh->do($stmt);
    my $curtime = time();

    my $sth = $dbh->prepare('INSERT INTO files (filename, description, downloadcount, uploadedby, uploaddate) VALUES($1, $2, 0, "Tic Processor", $3)');
    $sth->execute($_[1], $_[2], $curtime);

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
        if ($line =~ s/\r$//) {
            chop($line);
        }
        if (uc(substr($line, 0, 5)) eq "AREA ") {
            $area = substr($line, 5);
        } elsif (uc(substr($line, 0, 4)) eq "FILE") {
            $file = substr($line, 5);
        } elsif (uc(substr($line, 0, 5)) eq "LFILE") {
            $lfile = substr($line, 6);
        } elsif (uc(substr($line, 0, 4)) eq "DESC") {
            $desc = substr($line, 5);
        } elsif (uc(substr($line, 0, 5)) eq "LDESC") {
            if (length($ldesc) == 0) {
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
    if (length($area) > 0) {

        if (uc($config->val(uc($area), "password")) == uc($password)) {
            if (length($crc) > 0) {
                my $ctx = Digest::CRC->new( type => 'crc32' );
                my $filename;

                if (length($lfile) > 0 && -f $config->val('main', 'inbound') . "/" . $lfile) {
                    $filename = $lfile;
                } else { 
                    $filename = $file;
                }

                open my $fh, '<:raw', $config->val('main', 'inbound') . "/" . $filename or die $!;
                $ctx->addfile(*$fh);
                close $fh;

                if (hex($crc) == hex($ctx->hexdigest)) {
                    # CRC32 matches.

                    if (length($replaces) > 0) {
                        remove_from_database($config->val(uc($area), "database"), $replaces);
                    }

                    # Copy file
                    copy($config->val('main', 'inbound') . "/" . $filename, $config->val(uc($area), "path") . "/" . $filename);
                    # add file to database
                    my $description;

                    if (length($ldesc) > 0) {
                        $description = $ldesc;
                    } elsif (length($desc) > 0) {
                        $description = $desc;
                    } else {
                        if (uc(substr($config->val('main', 'inbound') . "/" . $filename, -3)) == "ZIP") {
                            File::Path::make_path("/tmp/ticproc/$$");
                            system("unzip -jCLL \"" . $config->val('main', 'inbound') . "/" . $filename . "\" file_id.diz -d /tmp/ticproc/$$ > /dev/null 2>&1");
                            if ( -f "/tmp/ticproc/$$/file_id.diz") {
                                local $/=undef;
                                open FILE, "/tmp/ticproc/$$/file_id.diz" or die "Couldn't open file: $!";
                                binmode FILE;
                                $description = <FILE>;
                                close FILE;

                                $description =~ s/\r//g;
                                unlink("/tmp/ticproc/$$/file_id.diz");
                            } else {
                                $description = "No description.";    
                            }
                            File::Path::remove_tree("/tmp/ticproc/$$");
                        } else {
                            $description = "No description.";
                        }
                    }

                    add_to_database($config->val(uc($area), "database"), $config->val(uc($area), "path") . "/" . $filename, $description);

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