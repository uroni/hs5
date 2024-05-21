import { createTableColumn, DataGrid, DataGridBody, DataGridCell, DataGridHeader, DataGridHeaderCell, DataGridRow, Spinner, TableCellLayout, TableColumnDefinition } from '@fluentui/react-components';
import { Suspense } from 'react';
import { useQuery } from 'react-query';

const Buckets = () => {

  /*const statusResult = useQuery("status", urbackupServer.status, 
    {suspense: true});*/

  return (
    <>
    <Suspense fallback={<Spinner />}>
      <h3>Buckets</h3>
    </Suspense>
    </>
  );
};

export default Buckets;