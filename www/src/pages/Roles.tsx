import { Button, createTableColumn, DataGrid, DataGridBody, DataGridCell, DataGridHeader, DataGridHeaderCell, DataGridProps, DataGridRow, Dialog, DialogActions, DialogBody, DialogContent, DialogSurface, DialogTitle, DialogTrigger, Field, Input, makeStyles, Spinner, TableCellLayout, TableColumnDefinition, tokens } from '@fluentui/react-components';
import { FormEvent, startTransition, Suspense, useEffect, useState } from 'react';
import { useSuspenseQuery, useQueryClient } from "@tanstack/react-query";
import { TableWrapper } from '../components/TableWrapper';
import { Pagination, PaginationItemsPerPageSelector, usePagination } from '../components/Pagination';
import { filterBySearch, SearchBox, useFilteredBySearch } from '../components/SearchBox';
import { ApiError, postApiV1B64Be5124B034028A58913931942E205ListRoles, postApiV1B64Be5124B034028A58913931942E205AddRole, postApiV1B64Be5124B034028A58913931942E205RemoveRole } from '../api';
import { Pages, router, state } from '../App';
import { useSnapshot } from 'valtio';
import { AddRegular, DeleteRegular, DocumentRegular } from '@fluentui/react-icons';
import { HapiError } from '../errorapi/HapiError';

type RoleType = {
  id: string;
  roleName: string;
  system?: boolean;
};

function filterRoles(item: RoleType, search: string) {
  const searchableFields = {
    roleName: item.roleName,
  };

  return filterBySearch(search, searchableFields);
}

function navigateToRolePolicies(role: RoleType) {
  startTransition(() => {
    router.navigate(`/${Pages.RolePolicies}/${encodeURIComponent(role.roleName)}/${role.system ? "1" : "0"}/${encodeURIComponent(role.id)}`);
  });
}

const useStyles = makeStyles({
  gridActions: {
    display: "flex",
    gap: tokens.spacingHorizontalS,
    flexWrap: "wrap",
  },
  formRow: {
    display: "flex",
    gap: tokens.spacingHorizontalM,
    alignItems: "flex-end",
    marginBottom: tokens.spacingVerticalM,
  },
});

const Roles = () => {

  const classes = useStyles();
  const queryClient = useQueryClient();

  const snap = useSnapshot(state);

  const rolesResult = useSuspenseQuery({
    queryKey: ["roles", snap.session],
    queryFn: async () => { return await postApiV1B64Be5124B034028A58913931942E205ListRoles({requestBody: {ses: snap.session}}); },
  });

  const data = rolesResult.data!.roles as RoleType[];

  const [sortState, setSortState] =
    useState<Parameters<NonNullable<DataGridProps["onSortChange"]>>[1]>();

  const onSortChange: DataGridProps["onSortChange"] = (e) => {
    e.preventDefault();
  };

  const [sortedData, setSortedData] = useState(data);

  useEffect(() => {
    setSortedData(data);
  }, [data]);

  const { setSearch, filteredItems } = useFilteredBySearch<RoleType>(
    sortedData,
    filterRoles,
  );

  const { itemsPerPage, setItemsPerPage, pageData, page, setPage } =
    usePagination(filteredItems);

  // Add role state
  const [newRoleName, setNewRoleName] = useState('');
  const [isAdding, setIsAdding] = useState(false);
  const [addError, setAddError] = useState('');

  // Delete role state
  const [isDeleting, setIsDeleting] = useState<string | null>(null);
  const [deleteDialogOpen, setDeleteDialogOpen] = useState(false);
  const [roleToDelete, setRoleToDelete] = useState<RoleType | null>(null);

  const handleAddRole = async (e: FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    if (!newRoleName.trim()) return;
    
    setIsAdding(true);
    setAddError('');
    try {
      await postApiV1B64Be5124B034028A58913931942E205AddRole({
        requestBody: { ses: snap.session, roleName: newRoleName.trim() }
      });
      setNewRoleName('');
      await queryClient.invalidateQueries({ queryKey: ["roles"] });
    } catch (apiE) {
      if (apiE instanceof ApiError) {
        const e = apiE.body as HapiError;
        setAddError(e.msg || 'Failed to add role');
      } else {
        setAddError('Failed to add role');
      }
    } finally {
      setIsAdding(false);
    }
  };

  const handleDeleteRole = async (role: RoleType) => {
    setIsDeleting(role.id);
    try {
      await postApiV1B64Be5124B034028A58913931942E205RemoveRole({
        requestBody: { ses: snap.session, id: role.id }
      });
      await queryClient.invalidateQueries({ queryKey: ["roles"] });
    } catch (apiE) {
      if (apiE instanceof ApiError) {
        const e = apiE.body as HapiError;
        console.error('Failed to delete role:', e.msg);
      }
    } finally {
      setIsDeleting(null);
      setDeleteDialogOpen(false);
      setRoleToDelete(null);
    }
  };

  const columns: TableColumnDefinition<RoleType>[] = [
    createTableColumn<RoleType>({
      columnId: "roleName",
      renderHeaderCell: () => {
        return "Role Name";
      },
      compare: (a, b) => {
        return a.roleName.localeCompare(b.roleName);
      },
      renderCell: (item) => {
        return <TableCellLayout>{item.roleName}</TableCellLayout>;
      },
    }),
    createTableColumn<RoleType>({
      columnId: "action",
      renderHeaderCell: () => {
        return "Actions";
      },
      renderCell: (item) => {
        return (
          <TableCellLayout>
            <Button
              icon={<DocumentRegular />}
              onClick={(e) => {
                e.stopPropagation();
                navigateToRolePolicies(item);
              }}
            >
              Policies
            </Button>
            <Button
              icon={<DeleteRegular />}
              disabled={isDeleting === item.id || item.system}
              onClick={(e) => {
                e.stopPropagation();
                setRoleToDelete(item);
                setDeleteDialogOpen(true);
              }}
            >
              {isDeleting === item.id ? 'Deleting...' : 'Delete'}
            </Button>
          </TableCellLayout>
        );
      }
    }),
  ];

  return (
    <>
    <Suspense fallback={<Spinner />}>
      <TableWrapper>
        <h3>Roles</h3>
        
        {/* Add Role Form */}
        <form onSubmit={handleAddRole} className={classes.formRow}>
          <Field label="New Role Name" validationMessage={addError} validationState={addError ? "error" : "none"}>
            <Input
              value={newRoleName}
              onChange={(e) => setNewRoleName(e.target.value)}
              placeholder="Enter role name"
              disabled={isAdding}
            />
          </Field>
          <Button type="submit" icon={<AddRegular />} appearance="primary" disabled={isAdding || !newRoleName.trim()}>
            {isAdding ? 'Adding...' : 'Add Role'}
          </Button>
        </form>

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
              getRowId={(item) => item.id}
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
              <DataGridBody<RoleType>>
                {({ item, rowId }) => (
                  <DataGridRow<RoleType>
                    key={rowId}
                  >
                    {({ renderCell }) => (
                      <DataGridCell>{renderCell(item)}</DataGridCell>
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
        </div>
      </TableWrapper>

      {/* Delete Confirmation Dialog */}
      <Dialog open={deleteDialogOpen} onOpenChange={(_, data) => setDeleteDialogOpen(data.open)}>
        <DialogSurface>
          <DialogBody>
            <DialogTitle>Delete Role</DialogTitle>
            <DialogContent>
              Are you sure you want to delete the role "{roleToDelete?.roleName}"?
            </DialogContent>
            <DialogActions>
              <DialogTrigger disableButtonEnhancement>
                <Button appearance="secondary">Cancel</Button>
              </DialogTrigger>
              <Button
                appearance="primary"
                onClick={() => roleToDelete && handleDeleteRole(roleToDelete)}
                disabled={isDeleting !== null}
              >
                {isDeleting ? 'Deleting...' : 'Delete'}
              </Button>
            </DialogActions>
          </DialogBody>
        </DialogSurface>
      </Dialog>
    </Suspense>
    </>
  );
};

export default Roles;
