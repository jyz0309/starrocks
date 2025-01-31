# minutes_add

## Description

Add the specified minute from the date, accurate to the minute.

## Syntax

```Haskell
DATETIME minutes_add(DATETIME|DATE date, INT minutes);
```

## Parameters

- `date`: the start time. It must be of the DATETIME or DATE type.

- `minutes`: the added minute. It must be INT type, it could be greater, equal or less than zero.

## Return value

Returns a DATETIME value.

Returns NULL if either `date` or `minutes` is NULL.

## Examples

```Plain
select minutes_add('2022-01-01 01:01:01', 2);
+---------------------------------------+
| minutes_add('2022-01-01 01:01:01', 2) |
+---------------------------------------+
| 2022-01-01 01:03:01                   |
+---------------------------------------+

select minutes_add('2022-01-01 01:01:01', -1);
+----------------------------------------+
| minutes_add('2022-01-01 01:01:01', -1) |
+----------------------------------------+
| 2022-01-01 01:00:01                    |
+----------------------------------------+

select minutes_add('2022-01-01', 1);
+------------------------------+
| minutes_add('2022-01-01', 1) |
+------------------------------+
| 2022-01-01 00:01:00          |
+------------------------------+
```