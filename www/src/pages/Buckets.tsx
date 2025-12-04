import { Button, createTableColumn, DataGrid, DataGridBody, DataGridCell, DataGridHeader, DataGridHeaderCell, DataGridProps, DataGridRow, makeStyles, Spinner, TableCellLayout, TableColumnDefinition, tokens } from '@fluentui/react-components';
import { FormEvent, Suspense, useEffect, useState } from 'react';
import { useQuery, useSuspenseQuery } from "@tanstack/react-query";
import { TableWrapper } from '../components/TableWrapper';
import { Pagination, PaginationItemsPerPageSelector, usePagination } from '../components/Pagination';
import { filterBySearch, SearchBox, useFilteredBySearch } from '../components/SearchBox';
import {postApiV1B64Be5124B034028A58913931942E205List, PostApiV1B64Be5124B034028A58913931942E205ListResponse} from '../api';
import { Pages, router, state } from '../App';
import { useSnapshot } from 'valtio';
import { AddRegular, DeleteRegular } from '@fluentui/react-icons';

type BucketType = {
  name: string;
  size?: number;
};

function filterBuckets(item: BucketType, search: string) {

  // Search in fields as displayed in the table
  const searchableFields = {
    name: item.name,
  };

  return filterBySearch(search, searchableFields);
}

const columns: TableColumnDefinition<BucketType>[] = [
  createTableColumn<BucketType>({
    columnId: "name",
    renderHeaderCell: () => {
      return "Bucket name";
    },
    compare: (a, b) => {
      return a.name.localeCompare(b.name);
    },
    renderCell: (item) => {
      return <TableCellLayout>{item.name}</TableCellLayout>;
    },
  }),
  createTableColumn<BucketType>({
    columnId: "action",
    renderHeaderCell: () => {
      return "Actions";
    },
    renderCell: (item) => {
      return <TableCellLayout><Button icon={<DeleteRegular />}>Delete</Button></TableCellLayout>;
    }
  }),
];

const useStyles = makeStyles({
  gridActions: {
    display: "flex",
    gap: tokens.spacingHorizontalS,
    flexWrap: "wrap",
  },
});

function browseToBucket(item: BucketType)
{
  localStorage.setItem("bucket", item.name);
  window.open("explorer-87652e131c4b45bd851a74de6518889e/explorer.html", "_blank");
}

const Buckets = () => {

  const classes = useStyles();

  const snap = useSnapshot(state);

  const bucketsResult = useSuspenseQuery({
    queryKey: ["buckets", snap.session],
    queryFn: async () => { return await postApiV1B64Be5124B034028A58913931942E205List({requestBody: {ses: snap.session, path: "/"}}); },
  });

  const data = bucketsResult.data!.objects as BucketType[];

  const [sortState, setSortState] =
    useState<Parameters<NonNullable<DataGridProps["onSortChange"]>>[1]>();

  const onSortChange: DataGridProps["onSortChange"] = (e) => {
    e.preventDefault();
  };

  const [sortedData, setSortedData] = useState(data);

  useEffect(() => {
    setSortedData(data);
  }, [data]);

  const { setSearch, filteredItems } = useFilteredBySearch<BucketType>(
    sortedData,
    filterBuckets,
  );

  const { itemsPerPage, setItemsPerPage, pageData, page, setPage } =
    usePagination(filteredItems);

  return (
    <>
    <Suspense fallback={<Spinner />}>
      <TableWrapper>
        <h3>Buckets</h3>
        <div className="cluster">
          <SearchBox onSearch={setSearch} />
          <PaginationItemsPerPageSelector
            itemsPerPage={itemsPerPage}
            setItemsPerPage={setItemsPerPage}
          />
          <DataGrid
              key={`${sortState?.sortColumn}-${sortState?.sortDirection}`}
              sortable
              onSortChange={onSortChange}
              items={pageData[page]}
              getRowId={(item) => item.name}
              columns={columns}
            >
              <DataGridHeader>
                <DataGridRow>
                  {({ renderHeaderCell, columnId, compare }) => (
                    <DataGridHeaderCell
                      sortDirection={
                        sortState?.sortColumn === columnId
                          ? sortState.sortDirection
                          : undefined
                      }
                      button={{
                        onClick: () => {
                          const newSortState: typeof sortState = {
                            sortColumn: columnId,
                            sortDirection:
                              sortState?.sortDirection === "descending" ||
                              sortState?.sortColumn !== columnId
                                ? "ascending"
                                : "descending",
                          };

                          if (newSortState.sortDirection === "ascending") {
                            const newSortedData = sortedData.sort(compare);
                            setSortedData(newSortedData);
                          }

                          if (newSortState.sortDirection === "descending") {
                            const newSortedData = sortedData.sort((a, b) =>
                              // Reverse compare function params for descending order
                              compare(b, a),
                            );
                            setSortedData(newSortedData);
                          }

                          setSortState(newSortState);
                        },
                      }}
                    >
                      {renderHeaderCell()}
                    </DataGridHeaderCell>
                  )}
                </DataGridRow>
              </DataGridHeader>
              <DataGridBody<BucketType>>
                {({ item }) => (
                  <DataGridRow<BucketType>
                    key={item.name}
                  >
                    {({ renderCell }) => (
                      <DataGridCell onClick={ () => browseToBucket(item) }>{renderCell(item)}</DataGridCell>
                    )}
                  </DataGridRow>
                )}
              </DataGridBody>
            </DataGrid>
            <Pagination
              pageCount={pageData.length}
              page={page}
              itemsPerPage={itemsPerPage}
              totalItemCount={filteredItems.length}
              setPage={setPage}
            />
            <div className={classes.gridActions}>
              <Button onClick={() => {
                state.pageAfterLogin = Pages.AddBucket;
                router.navigate(`/${state.pageAfterLogin}`);
              }} icon={<AddRegular />}>
                Add Bucket
              </Button>
            </div>
        </div>
      </TableWrapper>
    </Suspense>
    </>
  );
};

export default Buckets;