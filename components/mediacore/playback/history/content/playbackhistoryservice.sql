drop table if exists playback_history_entries;
drop table if exists properties;
drop table if exists playback_history_entry_annotations;

create table playback_history_entries (
  entry_id integer primary key autoincrement, /*implicit index creation*/
  library_guid text unique not null,          /*implicit index creation*/
  media_item_guid text unique not null,       /*implicit index creation*/
  play_time integer not null,
  play_duration integer
);

create index idx_playback_history_entries_play_time on playback_history_entries (play_time);

create table properties (
  property_id integer primary key autoincrement, /* implicit index creation */
  property_name text not null unique             /* implicit index creation */
);

create table playback_history_entry_annotations (
  entry_id integer not null,
  property_id integer not null,
  obj text not null,
  obj_sortable text,
  primary key (entry_id, property_id)
);

create index idx_playback_history_entry_annotations_entry_id (entry_id);
create index idx_playback_history_entry_annotations_entry_id_property_id (entry_id, property_id);
create index idx_playback_history_entry_annotations_obj_sortable (obj_sortable);
