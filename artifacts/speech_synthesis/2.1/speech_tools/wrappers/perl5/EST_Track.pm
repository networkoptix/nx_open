# This file was automatically generated by SWIG
package EST_Track;
require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);
require EST_FVector;
package EST_Trackc;
bootstrap EST_Track;
package EST_Track;
@EXPORT = qw( );

# ---------- BASE METHODS -------------

package EST_Track;

sub TIEHASH {
    my ($classname,$obj) = @_;
    return bless $obj, $classname;
}

sub CLEAR { }

sub FIRSTKEY { }

sub NEXTKEY { }

sub FETCH {
    my ($self,$field) = @_;
    my $member_func = "swig_${field}_get";
    $self->$member_func();
}

sub STORE {
    my ($self,$field,$newval) = @_;
    my $member_func = "swig_${field}_set";
    $self->$member_func($newval);
}

sub this {
    my $ptr = shift;
    return tied(%$ptr);
}


# ------- FUNCTION WRAPPERS --------

package EST_Track;

*mean = *EST_Trackc::mean;
*meansd = *EST_Trackc::meansd;
*normalise = *EST_Trackc::normalise;

############# Class : EST_Track::EST_Track ##############

package EST_Track::EST_Track;
@ISA = qw( EST_Track );
%OWNER = ();
%ITERATORS = ();
*swig_default_frame_shift_get = *EST_Trackc::EST_Track_default_frame_shift_get;
*swig_default_frame_shift_set = *EST_Trackc::EST_Track_default_frame_shift_set;
*swig_default_sample_rate_get = *EST_Trackc::EST_Track_default_sample_rate_get;
*swig_default_sample_rate_set = *EST_Trackc::EST_Track_default_sample_rate_set;
sub new {
    my $pkg = shift;
    my $self = EST_Trackc::new_EST_Track(@_);
    bless $self, $pkg if defined($self);
}

sub DESTROY {
    return unless $_[0]->isa('HASH');
    my $self = tied(%{$_[0]});
    return unless defined $self;
    delete $ITERATORS{$self};
    if (exists $OWNER{$self}) {
        EST_Trackc::delete_EST_Track($self);
        delete $OWNER{$self};
    }
}

*resize = *EST_Trackc::EST_Track_resize;
*set_num_channels = *EST_Trackc::EST_Track_set_num_channels;
*set_num_frames = *EST_Trackc::EST_Track_set_num_frames;
*set_channel_name = *EST_Trackc::EST_Track_set_channel_name;
*set_aux_channel_name = *EST_Trackc::EST_Track_set_aux_channel_name;
*copy_setup = *EST_Trackc::EST_Track_copy_setup;
*name = *EST_Trackc::EST_Track_name;
*set_name = *EST_Trackc::EST_Track_set_name;
*frame = *EST_Trackc::EST_Track_frame;
*channel = *EST_Trackc::EST_Track_channel;
*sub_track = *EST_Trackc::EST_Track_sub_track;
*copy_sub_track = *EST_Trackc::EST_Track_copy_sub_track;
*copy_sub_track_out = *EST_Trackc::EST_Track_copy_sub_track_out;
*copy_channel_out = *EST_Trackc::EST_Track_copy_channel_out;
*copy_frame_out = *EST_Trackc::EST_Track_copy_frame_out;
*copy_channel_in = *EST_Trackc::EST_Track_copy_channel_in;
*copy_frame_in = *EST_Trackc::EST_Track_copy_frame_in;
*channel_position = *EST_Trackc::EST_Track_channel_position;
*has_channel = *EST_Trackc::EST_Track_has_channel;
*a = *EST_Trackc::EST_Track_a;
*t = *EST_Trackc::EST_Track_t;
*ms_t = *EST_Trackc::EST_Track_ms_t;
*fill_time = *EST_Trackc::EST_Track_fill_time;
*fill = *EST_Trackc::EST_Track_fill;
*sample = *EST_Trackc::EST_Track_sample;
*shift = *EST_Trackc::EST_Track_shift;
*start = *EST_Trackc::EST_Track_start;
*end = *EST_Trackc::EST_Track_end;
*load = *EST_Trackc::EST_Track_load;
*save = *EST_Trackc::EST_Track_save;
*empty = *EST_Trackc::EST_Track_empty;
*index = *EST_Trackc::EST_Track_index;
*index_below = *EST_Trackc::EST_Track_index_below;
*num_frames = *EST_Trackc::EST_Track_num_frames;
*length = *EST_Trackc::EST_Track_length;
*num_channels = *EST_Trackc::EST_Track_num_channels;
*num_aux_channels = *EST_Trackc::EST_Track_num_aux_channels;
*equal_space = *EST_Trackc::EST_Track_equal_space;
*single_break = *EST_Trackc::EST_Track_single_break;
*set_equal_space = *EST_Trackc::EST_Track_set_equal_space;
*set_single_break = *EST_Trackc::EST_Track_set_single_break;
*load_channel_names = *EST_Trackc::EST_Track_load_channel_names;
*save_channel_names = *EST_Trackc::EST_Track_save_channel_names;
*channel_name = *EST_Trackc::EST_Track_channel_name;
*aux_channel_name = *EST_Trackc::EST_Track_aux_channel_name;
sub DISOWN {
    my $self = shift;
    my $ptr = tied(%$self);
    delete $OWNER{$ptr};
}

sub ACQUIRE {
    my $self = shift;
    my $ptr = tied(%$self);
    $OWNER{$ptr} = 1;
}


# ------- VARIABLE STUBS --------

package EST_Track;

1;
