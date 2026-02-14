import { Button, createTableColumn, DataGrid, DataGridBody, DataGridCell, DataGridHeader, DataGridHeaderCell, DataGridProps, DataGridRow, Dialog, DialogActions, DialogBody, DialogContent, DialogSurface, DialogTitle, DialogTrigger, Dropdown, Field, makeStyles, Option, Spinner, TableCellLayout, TableColumnDefinition, tokens } from '@fluentui/react-components';
import { FormEvent, Suspense, useEffect, useState } from 'react';
import { useSuspenseQuery, useQueryClient } from "@tanstack/react-query";
import { useParams } from 'react-router-dom';
import { TableWrapper } from '../components/TableWrapper';
import { Pagination, PaginationItemsPerPageSelector, usePagination } from '../components/Pagination';
import { filterBySearch, SearchBox, useFilteredBySearch } from '../components/SearchBox';
import { ApiError, postApiV1B64Be5124B034028A58913931942E205ListUserRoles, postApiV1B64Be5124B034028A58913931942E205AddUserRole, postApiV1B64Be5124B034028A58913931942E205RemoveUserRole, postApiV1B64Be5124B034028A58913931942E205ListRoles } from '../api';
import { state } from '../App';
import { useSnapshot } from 'valtio';
import { AddRegular, DeleteRegular } from '@fluentui/react-icons';
import { HapiError } from '../errorapi/HapiError';

type UserRoleType = {
  roleId: string;
  roleName: string;
  system?: boolean;
};

type RoleType = {
  id: string;
  roleName: string;
};

function filterUserRoles(item: UserRoleType, search: string) {
  const searchableFields = {
    roleName: item.roleName,
  };

  return filterBySearch(search, searchableFields);
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

const UserRoles = () => {

  const classes = useStyles();
  const queryClient = useQueryClient();
  const { username, userId } = useParams<{ username: string; userId: string }>();

  const snap = useSnapshot(state);

  const userRolesResult = useSuspenseQuery({
    queryKey: ["userRoles", snap.session, userId],
    queryFn: async () => { return await postApiV1B64Be5124B034028A58913931942E205ListUserRoles({requestBody: {ses: snap.session, userId: userId!}}); },
  });

  const rolesResult = useSuspenseQuery({
    queryKey: ["roles", snap.session],
    queryFn: async () => { return await postApiV1B64Be5124B034028A58913931942E205ListRoles({requestBody: {ses: snap.session}}); },
  });

  const data = (userRolesResult.data!.userRoles as { id: string; roleName: string; system?: boolean }[]).map(r => ({ roleId: r.id, roleName: r.roleName, system: r.system }));
  const allRoles = rolesResult.data!.roles as RoleType[];

  const [sortState, setSortState] =
    useState<Parameters<NonNullable<DataGridProps["onSortChange"]>>[1]>();

  const onSortChange: DataGridProps["onSortChange"] = (e) => {
    e.preventDefault();
  };

  const [sortedData, setSortedData] = useState(data);

  useEffect(() => {
    setSortedData(data);
  }, [data]);

  const { setSearch, filteredItems } = useFilteredBySearch<UserRoleType>(
    sortedData,
    filterUserRoles,
  );

  const { itemsPerPage, setItemsPerPage, pageData, page, setPage } =
    usePagination(filteredItems);

  // Add role state
  const [selectedRoleId, setSelectedRoleId] = useState<string>('');
  const [isAdding, setIsAdding] = useState(false);
  const [addError, setAddError] = useState('');

  // Remove role state
  const [isRemoving, setIsRemoving] = useState<string | null>(null);
  const [removeDialogOpen, setRemoveDialogOpen] = useState(false);
  const [roleToRemove, setRoleToRemove] = useState<UserRoleType | null>(null);

  const handleAddRole = async (e: FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    if (!selectedRoleId) return;
    
    setIsAdding(true);
    setAddError('');
    try {
      await postApiV1B64Be5124B034028A58913931942E205AddUserRole({
        requestBody: { ses: snap.session, userId: userId!, roleId: selectedRoleId }
      });
      setSelectedRoleId('');
      await queryClient.invalidateQueries({ queryKey: ["userRoles", snap.session, userId] });
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

  const handleRemoveRole = async (userRole: UserRoleType) => {
    setIsRemoving(userRole.roleId);
    try {
      await postApiV1B64Be5124B034028A58913931942E205RemoveUserRole({
        requestBody: { ses: snap.session, id: userRole.roleId }
      });
      await queryClient.invalidateQueries({ queryKey: ["userRoles", snap.session, userId] });
    } catch (apiE) {
      if (apiE instanceof ApiError) {
        const e = apiE.body as HapiError;
        console.error('Failed to remove role:', e.msg);
      }
    } finally {
      setIsRemoving(null);
      setRemoveDialogOpen(false);
      setRoleToRemove(null);
    }
  };

  const columns: TableColumnDefinition<UserRoleType>[] = [
    createTableColumn<UserRoleType>({
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
    createTableColumn<UserRoleType>({
      columnId: "action",
      renderHeaderCell: () => {
        return "Actions";
      },
      renderCell: (item) => {
        return (
          <TableCellLayout>
            <Button
              icon={<DeleteRegular />}
              disabled={isRemoving === item.roleId || item.system}
              onClick={(e) => {
                e.stopPropagation();
                setRoleToRemove(item);
                setRemoveDialogOpen(true);
              }}
            >
              {isRemoving === item.roleId ? 'Removing...' : 'Remove'}
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
        <h3>Roles for user: {username}</h3>
        
        {/* Add Role Form */}
        <form onSubmit={handleAddRole} className={classes.formRow}>
          <Field label="Add Role" validationMessage={addError} validationState={addError ? "error" : "none"}>
            <Dropdown
              placeholder="Select a role"
              value={allRoles.find(r => r.id === selectedRoleId)?.roleName || ''}
              onOptionSelect={(_, data) => setSelectedRoleId(data.optionValue || '')}
              disabled={isAdding}
            >
              {allRoles.map((role) => (
                <Option key={role.id} value={role.id}>
                  {role.roleName}
                </Option>
              ))}
            </Dropdown>
          </Field>
          <Button type="submit" icon={<AddRegular />} appearance="primary" disabled={isAdding || !selectedRoleId}>
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
              getRowId={(item) => item.roleId}
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
              <DataGridBody<UserRoleType>>
                {({ item, rowId }) => (
                  <DataGridRow<UserRoleType>
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

      {/* Remove Confirmation Dialog */}
      <Dialog open={removeDialogOpen} onOpenChange={(_, data) => setRemoveDialogOpen(data.open)}>
        <DialogSurface>
          <DialogBody>
            <DialogTitle>Remove Role</DialogTitle>
            <DialogContent>
              Are you sure you want to remove the role "{roleToRemove?.roleName}" from this user?
            </DialogContent>
            <DialogActions>
              <DialogTrigger disableButtonEnhancement>
                <Button appearance="secondary">Cancel</Button>
              </DialogTrigger>
              <Button
                appearance="primary"
                onClick={() => roleToRemove && handleRemoveRole(roleToRemove)}
                disabled={isRemoving !== null}
              >
                {isRemoving ? 'Removing...' : 'Remove'}
              </Button>
            </DialogActions>
          </DialogBody>
        </DialogSurface>
      </Dialog>
    </Suspense>
    </>
  );
};

export default UserRoles;
