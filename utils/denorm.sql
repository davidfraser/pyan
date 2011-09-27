/*
 * denorm(index, value) aggregate function.
 *
 * Constructs an array of its input pairs, with each index of the array set to
 * the corresponding value.
 *
 * The intended use is to help with "denormalising" a table by collecting
 * several scalar values into an array.  Works best if the indices are close
 * in proximity (since gaps between will be filled with NULLs).
 *
 * Example:
 *
 * Original table: log (time TIMESTAMP, sensor_id INTEGER, value DOUBLE PRECISION)
 * SELECT * FROM log;
 *     2011-01-01, 1, 4.14
 *     2011-01-01, 2, 7.12
 *     2011-01-02, 1, 9.05
 *     ...
 * CREATE TABLE hlog AS SELECT time, denorm(sensor_id, value) FROM log;
 *
 * New table is hlog (time TIMESTAMP, values DOUBLE PRECISION[])
 * SELECT * FROM hlog;
 *     2011-01-01, {4.14, 7.12}
 *     2011-01-01, {9.05, ...}
 *     ...
 *
 * Notes:
 *   * denorm of no rows yields empty array (rather than NULL).
 *   * denorm(NULL, x) results in error ("array subscript in assignment must
 *     not be null"). 
 *   * denorm(i, NULL) sets that index to NULL in the result array.
 *   * If the same index appears more than once in the input, the value that
 *     gets fed to the function last will take effect.
 *   * You may need to explicitly specify the type of the value argument to
 *     denorm, so that it knows what kind of array to construct.
 */
CREATE OR REPLACE FUNCTION denorm(ANYARRAY, INTEGER, ANYELEMENT) RETURNS ANYARRAY
IMMUTABLE
LANGUAGE 'plpgsql'
AS $$BEGIN
    $1[$2] = $3;
    return $1;
END;$$;
COMMENT ON FUNCTION denorm(ANYARRAY, INTEGER, ANYELEMENT) IS 'Transition function for aggregate denorm(INTEGER, ANYELEMENT).';


CREATE AGGREGATE denorm(INTEGER, ANYELEMENT)
(
    SFUNC = denorm,
    STYPE = ANYARRAY,
    INITCOND = '{}'
);
COMMENT ON AGGREGATE denorm(INTEGER, ANYELEMENT) IS 'denorm(index, value) returns an array constructed by populating each index with its corresponding value from the input.';
