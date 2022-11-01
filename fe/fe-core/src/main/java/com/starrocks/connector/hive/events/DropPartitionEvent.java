// This file is licensed under the Elastic License 2.0. Copyright 2021-present, StarRocks Inc.

package com.starrocks.connector.hive.events;

import com.google.common.base.Preconditions;
import com.google.common.collect.Lists;
import com.starrocks.catalog.Column;
import com.starrocks.connector.hive.CacheUpdateProcessor;
import org.apache.hadoop.hive.metastore.api.NotificationEvent;
import org.apache.hadoop.hive.metastore.messaging.DropPartitionMessage;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

import java.util.List;
import java.util.Map;

/**
 * MetastoreEvent for DROP_PARTITION event type
 */
public class DropPartitionEvent extends MetastoreTableEvent {
    private static final Logger LOG = LogManager.getLogger(DropPartitionEvent.class);
    public static final String EVENT_TYPE = "DROP_PARTITION";

    private final Map<String, String> droppedPartition;
    // partCols use to generate HivePartitionKeysKey
    private final List<Column> partCols;

    private DropPartitionEvent(NotificationEvent event,
                               CacheUpdateProcessor metaCache,
                               Map<String, String> droppedPartition,
                               List<Column> partCols) {
        super(event, metaCache);
        Preconditions.checkState(getEventType().equals(MetastoreEventType.DROP_PARTITION));
        Preconditions.checkNotNull(event.getMessage());
        DropPartitionMessage dropPartitionMessage =
                MetastoreEventsProcessor.getMessageDeserializer()
                        .getDropPartitionMessage(event.getMessage());
        try {
            hmsTbl = Preconditions.checkNotNull(dropPartitionMessage.getTableObj());
            Preconditions.checkNotNull(droppedPartition);
            this.droppedPartition = droppedPartition;
            Preconditions.checkState(!partCols.isEmpty());
            this.partCols = partCols;
            hivePartitionKeys.clear();
            // TODO(stephen): refactor this function
            // hivePartitionKeys.add(new HivePartitionName(dbName, tblName, Table.TableType.HIVE, Lists.newArrayList(droppedPartition.values())));
        } catch (Exception ex) {
            throw new MetastoreNotificationException(
                    debugString("Could not parse drop event message. "), ex);
        }
    }

    protected static List<MetastoreEvent> getEvents(NotificationEvent event,
                                                    CacheUpdateProcessor metaCache,
                                                    List<Column> partCols) {
        DropPartitionMessage dropPartitionMessage =
                MetastoreEventsProcessor.getMessageDeserializer()
                        .getDropPartitionMessage(event.getMessage());
        List<MetastoreEvent> dropPartitionEvents = Lists.newArrayList();
        try {
            List<Map<String, String>> droppedPartitions = dropPartitionMessage.getPartitions();
            droppedPartitions.forEach(part ->
                    dropPartitionEvents.add(new DropPartitionEvent(event, metaCache, part, partCols)));
        } catch (Exception e) {
            throw new MetastoreNotificationException(e);
        }
        return dropPartitionEvents;
    }

    @Override
    public boolean canBeBatched(MetastoreEvent event) {
        return true;
    }

    @Override
    protected MetastoreEvent addToBatchEvents(MetastoreEvent event) {
        BatchEvent<MetastoreTableEvent> batchEvent = new BatchEvent<>(this);
        Preconditions.checkState(batchEvent.canBeBatched(event));
        batchEvent.addToBatchEvents(event);
        return batchEvent;
    }

    @Override
    protected boolean existInCache() {
        return true;
    }

    @Override
    protected boolean isSupported() {
        return true;
    }

    @Override
    protected void process() throws MetastoreNotificationException {
        try {
            // TODO(stephen): refactor this function
            // HivePartitionKeysKey partitionKeysKey = new HivePartitionKeysKey(dbName, tblName, Table.TableType.HIVE, partCols);
            // PartitionKey partitionKey = PartitionUtil.createPartitionKey(Lists.newArrayList(droppedPartition.values()), partCols);
            // cache.dropPartitionKeyByEvent(partitionKeysKey, partitionKey, getHivePartitionKey());
        } catch (Exception e) {
            LOG.error("Failed to process {} event, event detail msg: {}",
                    getEventType(), metastoreNotificationEvent, e);
            throw new MetastoreNotificationException(
                    debugString("Failed to process drop partition event"));
        }
    }
}