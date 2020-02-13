#!/usr/bin/perl 

my $dpi_info = `xdpyinfo | grep dimensions`;


if( $dpi_info =~ /dimensions:\s*(\d+)x(\d+)/)
{
    #print "Hello: ${1} - ${2}\n ";
    my @args = ("${1}", "${2}");

    exec( './CollidoscopeApp', @args);
}

